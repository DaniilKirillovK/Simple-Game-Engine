#version 330 core

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vTexCoord;

out vec4 FragColor;

uniform vec4 uMaterialDiffuseColor;
uniform vec4 uMaterialSpecularColor;
uniform vec4 uMaterialAmbientColor;
uniform float uMaterialShininess;
uniform int uMaterialHasTexture;

// Light
struct Light {
    int type;      // 0-directional, 1-point, 2-spot
    vec3 color;
    float intensity;
    vec3 position;
    vec3 direction;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
};

uniform int uNumLights;
uniform Light uLights[8];

// Camera
uniform vec3 uCameraPosition;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(uCameraPosition - vWorldPosition);

    vec3 result = vec3(0.0);

    vec3 globalAmbient = vec3(0.2);
    result += globalAmbient * uMaterialDiffuseColor.rgb;

    for (int i = 0; i < uNumLights; i++)
    {
        Light light = uLights[i];

        vec3 lightDir;
        float attenuation = 1.0;

        if (light.type == 0) { // Directional light
            lightDir = normalize(-light.direction);
        }
        else if (light.type == 1) { // Point light
            vec3 lightVec = light.position - vWorldPosition;
            float distance = length(lightVec);
            lightDir = normalize(lightVec);
            attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
        }
        else { // Spot light
            vec3 lightVec = light.position - vWorldPosition;
            float distance = length(lightVec);
            lightDir = normalize(lightVec);

            float theta = dot(lightDir, normalize(-light.direction));
            float epsilon = light.cutOff - light.outerCutOff;
            float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

            attenuation = intensity / (light.constant + light.linear * distance + light.quadratic * distance * distance);
        }

        // Ambient
        vec3 ambient = light.color * light.intensity * uMaterialAmbientColor.rgb;

        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = light.color * light.intensity * diff * uMaterialDiffuseColor.rgb;

        // Specular
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), uMaterialShininess);
        vec3 specular = light.color * light.intensity * spec * uMaterialSpecularColor.rgb;

        result += (ambient + diffuse + specular) * attenuation;
    }

    FragColor = vec4(result, uMaterialDiffuseColor.a);
}