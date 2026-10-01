#include "Resources/AssetLoader.h"

#include "IRenderAdapter.h"
#include "Logger.h"
#include "Components/MeshRenderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/MeshLoader/MeshLoader.h"
#include "Resources/TextureLoader/Texture.h"
#include "Resources/TextureLoader/TextureLoader.h"

#include "tracy/Tracy.hpp"

#include <chrono>
#include <thread>

AssetLoader::AssetLoader() = default;
AssetLoader::~AssetLoader() = default;

void AssetLoader::initialize(JobSystem* jobs, IRenderAdapter* renderer)
{
    m_jobs = jobs;
    m_renderer = renderer;
    m_stopping.store(false);

    // Создаём ОДНУ общую белую текстуру 1x1: её handle получают все текстуры, которые ещё
    // грузятся. Вызов createTexture — это работа с OpenGL, поэтому initialize обязан
    // выполняться в главном потоке.
    // Нюанс: деструктор TextureData освобождает пиксели через stbi_image_free, а здесь буфер
    // лежит на стеке. Поэтому после создания текстуры указатель pixels обнуляется вручную,
    // иначе деструктор попытался бы освободить память стека.
    unsigned char white[4] = { 255, 255, 255, 255 };
    TextureData whitePixel(1, 1, 4, white);
    m_placeholderTexture = m_renderer->createTexture(whitePixel);
    whitePixel.pixels = nullptr;

    LOG_INFO("AssetLoader initialized");
}

void AssetLoader::shutdown()
{
    if (!m_jobs)
    {
        return;
    }

    // TRACY: время завершения работы при выходе (ожидание задач). Должно быть небольшим —
    // это измеримое подтверждение критерия «выход с живыми задачами не виснет».
    ZoneScopedN("Asset::Shutdown");

    // Шаг 1. Флаг остановки. Задачи, которые лежат в очереди job system, но ещё не начали
    // работу, увидят его в decodeJob, пропустят тяжёлое декодирование и сразу завершатся.
    // Так выход из приложения не ждёт декодирования файлов, которые уже никому не нужны.
    m_stopping.store(true);

    // Шаг 2. Ожидание. Воркеры в этот момент ещё живы (вызывающий код останавливает job system
    // только после shutdown), поэтому все отправленные задачи быстро доходят до конца.
    // Таймаут 10 секунд нужен как предохранитель от зависания на выходе (требование ТЗ:
    // «выход с живыми задачами не должен виснуть»); при его срабатывании пишем ошибку в лог.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (m_inFlight.load() > 0)
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            LOG_ERROR("AssetLoader: shutdown timed out with decode jobs still running");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Шаг 3. Выбрасываем результаты, до которых pump() уже не доберётся
    // (вместе с ними освобождаются shared_ptr на заглушки).
    {
        std::lock_guard<std::mutex> lock(m_readyMutex);
        m_ready.clear();
    }
    m_pending.store(0);
    m_jobs = nullptr;

    LOG_INFO("AssetLoader shut down");
}

std::shared_ptr<Resource<Mesh>> AssetLoader::requestMesh(const std::string& path)
{
    // TRACY: время самого запроса в главном потоке. Должно быть микросекунды — это и есть
    // доказательство, что запрос не блокирует кадр (в отличие от синхронного load).
    ZoneScopedN("Asset::RequestMesh");
    ZoneText(path.c_str(), path.size());

    // Не принимаем запросы с пустым путём, до initialize, а также во время остановки.
    if (path.empty() || !m_jobs || m_stopping.load())
    {
        return nullptr;
    }

    // Путь уже известен (грузится, готов или упал с ошибкой) — отдаём тот же самый объект.
    // Благодаря этому два запроса одного файла не создают двух загрузок и двух копий меша.
    // Безопасно без гонки, потому что request* вызывается только из главного потока.
    if (auto existing = RESOURCE_MANAGER.get<Mesh>(path))
    {
        return existing;
    }

    // Создаём ПУСТУЮ заглушку. ready = false заставляет RenderSystem и drawMesh её пропускать.
    auto placeholder = std::make_unique<Mesh>();
    placeholder->type = MeshType::Custom;
    placeholder->path = path;
    placeholder->ready = false;

    // insert создаёт Resource сразу в состоянии Ready (у него есть данные — наша заглушка),
    // поэтому состояние тут же исправляем на Loading.
    auto resource = RESOURCE_MANAGER.insert<Mesh>(path, std::move(placeholder));
    resource->setState(ResourceState::Loading);

    // Формируем запрос и отправляем декодирование в job system.
    auto request = std::make_unique<Request>();
    request->isMesh = true;
    request->path = path;
    request->meshResource = resource;
    dispatch(std::move(request));

    return resource;
}

std::shared_ptr<Resource<Texture>> AssetLoader::requestTexture(const std::string& path)
{
    ZoneScopedN("Asset::RequestTexture"); // TRACY: см. комментарий в requestMesh
    ZoneText(path.c_str(), path.size());

    if (path.empty() || !m_jobs || m_stopping.load())
    {
        return nullptr;
    }

    // Уже запрошенная/загруженная текстура: возвращаем существующий объект (см. requestMesh).
    if (auto existing = RESOURCE_MANAGER.get<Texture>(path))
    {
        return existing;
    }

    // Заглушка: handle указывает на общую белую текстуру 1x1, поэтому материал, получивший
    // этот объект, уже сейчас что-то рисует (белый цвет) и не ломается.
    auto placeholder = std::make_unique<Texture>();
    placeholder->m_handle = m_placeholderTexture;
    placeholder->m_width = 1;
    placeholder->m_height = 1;
    placeholder->m_channels = 4;
    placeholder->m_path = path;
    placeholder->ready = false;

    auto resource = RESOURCE_MANAGER.insert<Texture>(path, std::move(placeholder));
    resource->setState(ResourceState::Loading);

    auto request = std::make_unique<Request>();
    request->isMesh = false;
    request->path = path;
    request->textureResource = resource;
    dispatch(std::move(request));

    return resource;
}

void AssetLoader::dispatch(std::unique_ptr<Request> request)
{
    // Счётчики увеличиваем ДО отправки: иначе быстрая задача могла бы завершиться раньше,
    // чем мы их увеличили, и счётчики на мгновение показали бы неверное значение.
    m_pending.fetch_add(1);
    m_inFlight.fetch_add(1);

    // JobSystem::execute принимает только «функция + void*», поэтому владение запросом
    // передаётся вместе с «сырым» указателем (release). Обратно его заберёт decodeJob.
    m_jobs->execute(&AssetLoader::decodeJob, request.release());
}

// ---- сторона воркера ----

void AssetLoader::decodeJob(void* raw, uint32_t /*index*/)
{
    // Забираем владение запросом обратно в unique_ptr: что бы ни случилось ниже, память не утечёт.
    std::unique_ptr<Request> request(static_cast<Request*>(raw));
    AssetLoader& self = AssetLoader::getInstance();

    // Если идёт остановка, тяжёлое декодирование пропускаем (meshData/textureData останутся
    // пустыми), но запрос всё равно «завершаем» через complete, иначе shutdown ждал бы вечно.
    if (!self.m_stopping.load())
    {
        if (request->isMesh)
        {
            // Зона Tracy: на графике видно, какой воркер сколько декодирует меш.
            ZoneScopedN("Asset::DecodeMesh");
            // TRACY: подпись зоны — путь к файлу. В окне Tracy у каждой зоны видно, КАКОЙ
            // именно ресурс декодировался и на каком воркере, это нужно для разбора трейса.
            ZoneText(request->path.c_str(), request->path.size());
            // Только CPU: Assimp читает файл и собирает векторы вершин/индексов.
            // GPU-буферы здесь не создаются — рендерер сделает это лениво при первой отрисовке.
            request->meshData = MeshLoader::getInstance().loadMesh(request->path);
        }
        else
        {
            ZoneScopedN("Asset::DecodeTexture");
            ZoneText(request->path.c_str(), request->path.size()); // TRACY: имя файла в зоне
            // Стадия 1 загрузки текстуры: чтение и декодирование файла. Без OpenGL.
            request->textureData = TextureLoader::getInstance().loadFromFile(request->path);
        }
    }

    self.complete(request.release());
}

void AssetLoader::complete(Request* request)
{
    // Кладём результат в очередь готовых. Под мьютексом, потому что одновременно в неё могут
    // писать несколько воркеров, а читает главный поток в pump().
    {
        std::lock_guard<std::mutex> lock(m_readyMutex);
        m_ready.emplace_back(request);
    }

    // Это должно быть САМЫМ ПОСЛЕДНИМ действием задачи: shutdown() выходит из ожидания,
    // как только счётчик достигает нуля, и после этой строки задача уже ничего не трогает.
    m_inFlight.fetch_sub(1);
}

// ---- сторона главного потока ----

void AssetLoader::pump(float budgetMs)
{
    ZoneScopedN("Asset::Pump");
    // График в Tracy: сколько запросов сейчас в работе (видно «нагрузку» загрузки по времени).
    TracyPlot("Asset::Pending", static_cast<int64_t>(m_pending.load()));

    if (!m_jobs)
    {
        return;
    }

    const auto start = std::chrono::steady_clock::now();
    uint32_t processed = 0; // TRACY: сколько результатов завершено за этот вызов pump

    while (true)
    {
        // Достаём один готовый результат. Мьютекс держим только на время извлечения, а тяжёлую
        // работу (finish) делаем уже без него, чтобы воркеры могли свободно класть новые результаты.
        std::unique_ptr<Request> request;
        {
            std::lock_guard<std::mutex> lock(m_readyMutex);
            if (m_ready.empty())
            {
                break;
            }
            request = std::move(m_ready.front());
            m_ready.pop_front();
        }

        finish(*request);
        m_pending.fetch_sub(1);
        ++processed;

        // Проверка бюджета стоит ПОСЛЕ обработки: так за кадр гарантированно завершается
        // хотя бы один результат, и очередь не может «застрять» из-за слишком малого лимита.
        const float elapsedMs = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsedMs >= budgetMs)
        {
            break;
        }
    }

    // TRACY: графики по каждому кадру (Tracy рисует их под таймлайном).
    //   Asset::PumpProcessed — сколько ресурсов завершено за кадр;
    //   Asset::PumpMs        — сколько миллисекунд кадр потратил на pump. Должен упираться
    //                          в бюджет (2 мс) плюс один ресурс, а не расти вместе с нагрузкой.
    // Вызов стоит после цикла, поэтому значения пишутся в любом случае (в т.ч. 0, когда очередь пуста).
    const float totalMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    TracyPlot("Asset::PumpProcessed", static_cast<int64_t>(processed));
    TracyPlot("Asset::PumpMs", static_cast<double>(totalMs));
}

void AssetLoader::finish(Request& request)
{
    if (request.isMesh)
    {
        ZoneScopedN("Asset::FinishMesh");
        ZoneText(request.path.c_str(), request.path.size()); // TRACY: имя файла в зоне

        // Декодирование не удалось (файла нет / ошибка Assimp): помечаем ресурс Failed.
        // Заглушка остаётся пустой, указатели на неё не становятся висячими.
        if (!request.meshData)
        {
            LOG_ERROR("AssetLoader: failed to load mesh: " + request.path);
            request.meshResource->setState(ResourceState::Failed);
            return;
        }

        // Заполняем заглушку «на месте»: перемещаем данные в ТОТ ЖЕ объект Mesh, на который
        // уже ссылаются MeshRenderer'ы. В декодированном меше ready уже true (значение по
        // умолчанию), но выставляем явно для наглядности. GPU-буферы (VAO/VBO/EBO) создаст
        // рендерер лениво при первой отрисовке, отдельной GPU-стадии для меша не нужно.
        Mesh* mesh = request.meshResource->get();
        *mesh = std::move(*request.meshData);
        mesh->ready = true;
        request.meshResource->setState(ResourceState::Ready);
        return;
    }

    ZoneScopedN("Asset::FinishTexture");
    ZoneText(request.path.c_str(), request.path.size()); // TRACY: имя файла в зоне

    if (!request.textureData)
    {
        LOG_ERROR("AssetLoader: failed to load texture: " + request.path);
        request.textureResource->setState(ResourceState::Failed);
        return;
    }

    // Стадия 2 загрузки текстуры: пиксели -> видеопамять (glTexImage2D + mipmap). Это
    // вызовы OpenGL, поэтому выполняются здесь, в главном потоке, в рамках бюджета pump().
    auto uploaded = TextureLoader::getInstance().createGPU(*request.textureData, request.path);
    // Пиксели в оперативной памяти больше не нужны — освобождаем сразу, не дожидаясь конца запроса.
    request.textureData.reset();

    if (!uploaded)
    {
        request.textureResource->setState(ResourceState::Failed);
        return;
    }

    // Заполняем заглушку «на месте»: меняем handle, размеры и ready в ТОМ ЖЕ объекте Texture.
    // Все Material, у которых diffuseTexture указывает на него, начинают показывать
    // настоящую картинку без каких-либо дополнительных действий.
    Texture* texture = request.textureResource->get();
    *texture = std::move(*uploaded);
    texture->ready = true;
    request.textureResource->setState(ResourceState::Ready);
}
