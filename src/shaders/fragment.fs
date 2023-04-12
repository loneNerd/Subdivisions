#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D diffuse_1;

void main()
{
    if (TexCoords.x < 0 && TexCoords.y < 0)
        FragColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    else
        FragColor = texture(diffuse_1, TexCoords);
}
