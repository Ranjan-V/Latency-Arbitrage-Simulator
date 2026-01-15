#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 objectColor;
uniform bool useTexture;

void main() {
    // Ambient lighting
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * vec3(1.0);
    
    // Diffuse lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);
    
    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * vec3(1.0);
    
    // Ocean/land coloring based on texture coordinates
    vec3 baseColor;
    if (useTexture) {
        // Simple ocean (blue) vs land (green) based on latitude
        float landMask = sin(TexCoord.y * 3.14159 * 4.0) * cos(TexCoord.x * 3.14159 * 6.0);
        if (landMask > 0.3) {
            baseColor = vec3(0.2, 0.6, 0.2); // Land (green)
        } else {
            baseColor = vec3(0.1, 0.3, 0.8); // Ocean (blue)
        }
    } else {
        baseColor = objectColor;
    }
    
    vec3 result = (ambient + diffuse + specular) * baseColor;
    FragColor = vec4(result, 1.0);
}