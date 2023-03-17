#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D diffuse_1;
uniform sampler2D specular_1;
uniform sampler2D normal_1;
uniform sampler2D height_1;

void main()
{
    if (TexCoords.x < 0 && TexCoords.y < 0)
        FragColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    else
        FragColor = texture(diffuse_1, TexCoords);// * texture(specular_1, TexCoords) * texture(normal_1, TexCoords) * texture(height_1, TexCoords);
}
