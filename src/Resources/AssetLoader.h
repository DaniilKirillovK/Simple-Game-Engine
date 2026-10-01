#pragma once

#include "Resources/Resource.h"
#include "JobSystem/JobSystem.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

class IRenderAdapter;
struct Mesh;
struct Texture;
struct TextureData;

// НОВЫЙ КЛАСС. Асинхронная загрузка мешей и текстур поверх JobSystem.
//
// Идея: тяжёлая CPU-часть загрузки (разбор модели в Assimp, декодирование PNG/JPG в stb_image)
// уходит в воркеры, а главный поток только «закрывает» загрузку — отправляет данные в GPU
// малыми порциями. Так кадр не замирает, пока читается и декодируется файл.
//
//   главный поток                     воркер                          главный поток
//   -------------                     ------                          -------------
//   requestMesh/requestTexture  -->   decode (Assimp / stb_image) --> pump()
//   сразу возвращает заглушку         только CPU, без вызовов GL      загрузка в GPU, заполнение
//                                                                     объекта «на месте», Ready
//
// СТРАТЕГИЯ ЗАГЛУШКИ (placeholder). Объект Mesh / Texture создаётся сразу при запросе и
// регистрируется в ResourceManager, поэтому указатель, выданный request*(), остаётся
// валидным навсегда. Пока идёт загрузка:
//   - меш пустой (ready == false), RenderSystem такой меш пропускает;
//   - текстура указывает на общую белую GL-текстуру 1x1.
// Когда данные приходят, pump() заполняет ЭТОТ ЖЕ объект «на месте». Поэтому все Material и
// MeshRenderer, которые уже хранят указатель, видят результат автоматически, и менять
// ссылки в компонентах не приходится.
//
// ПОТОКИ. initialize / request* / pump / shutdown вызываются ТОЛЬКО из главного потока.
// В воркерах работают лишь задачи декодирования, и они никогда не трогают ни OpenGL,
// ни ResourceManager (поэтому им не нужны блокировки на этих объектах).
class AssetLoader
{
public:
    static AssetLoader& getInstance()
    {
        static AssetLoader instance;
        return instance;
    }

    void initialize(JobSystem* jobs, IRenderAdapter* renderer);

    // Останавливает приём новых запросов и ждёт, пока завершатся все уже отправленные задачи
    // декодирования. Обязательно вызывать, пока воркеры JobSystem ещё живы, иначе ждать будет некому.
    void shutdown();

    // Возвращает ресурс СРАЗУ, не блокируя. После initialize для непустого пути не возвращает
    // nullptr. Ресурс находится в состоянии Loading, пока pump() не завершит загрузку (Ready)
    // или пока декодирование не провалится (Failed). Повторный запрос того же пути вернёт тот
    // же самый объект, новых задач при этом не создаётся.
    std::shared_ptr<Resource<Mesh>> requestMesh(const std::string& path);
    std::shared_ptr<Resource<Texture>> requestTexture(const std::string& path);

    // «Памп»: завершает готовые запросы (GPU-часть, заполнение объектов). Всегда обрабатывает
    // минимум один результат (чтобы очередь гарантированно продвигалась), затем останавливается,
    // как только потрачен бюджет budgetMs миллисекунд. Это и есть «лимит на кадр» из ТЗ.
    void pump(float budgetMs);

    // Сколько запросов ещё в работе: декодируются или ждут своей очереди в pump().
    uint32_t pendingCount() const { return m_pending.load(); }

private:
    // Определены в .cpp: структура Request владеет unique_ptr на типы, которые в этом
    // заголовке только объявлены вперёд (Mesh, TextureData), и деструктору нужны их полные определения.
    AssetLoader();
    ~AssetLoader();

    // Один запрос на загрузку. Создаётся в главном потоке, «путешествует» через воркер и
    // возвращается обратно в главный поток через очередь m_ready.
    struct Request
    {
        bool isMesh = true;
        std::string path;

        // Заглушка, которую нужно будет заполнить (shared_ptr держит ресурс живым, даже если
        // его успели выгрузить из ResourceManager, пока шла загрузка).
        std::shared_ptr<Resource<Mesh>> meshResource;
        std::shared_ptr<Resource<Texture>> textureResource;

        // Результат работы воркера (CPU-данные). Заполняется в decodeJob.
        std::unique_ptr<Mesh> meshData;
        std::unique_ptr<TextureData> textureData;
    };

    // Функция задачи для JobSystem (сигнатура void(*)(void*, uint32_t) задана job system).
    // raw — указатель на Request, index здесь не используется.
    static void decodeJob(void* raw, uint32_t index);

    void dispatch(std::unique_ptr<Request> request);
    void complete(Request* request);  // сторона воркера: передать результат в pump()
    void finish(Request& request);    // сторона главного потока: GPU-часть и публикация результата

    JobSystem* m_jobs = nullptr;
    IRenderAdapter* m_renderer = nullptr;
    uint32_t m_placeholderTexture = 0; // GL-идентификатор общей белой текстуры 1x1

    // Флаг остановки: задачи, которые ещё не начали декодирование, при нём пропускают работу.
    std::atomic<bool> m_stopping{ false };
    // Запросов отправлено, но pump() их ещё не завершил (используется для графика в Tracy).
    std::atomic<uint32_t> m_pending{ 0 };
    // Задач декодирования отправлено, но воркер ещё не закончил. Именно по этому счётчику
    // shutdown() понимает, что ждать больше нечего.
    std::atomic<uint32_t> m_inFlight{ 0 };

    // Очередь готовых результатов: воркеры кладут сюда, главный поток забирает в pump().
    // Это единственное место, где воркер и главный поток обмениваются данными, поэтому
    // доступ к нему защищён мьютексом.
    std::mutex m_readyMutex;
    std::deque<std::unique_ptr<Request>> m_ready;
};
