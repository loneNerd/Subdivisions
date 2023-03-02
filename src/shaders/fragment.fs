#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D diffuse_1;
uniform sampler2D specular_1;
uniform sampler2D normal_1;
uniform sampler2D height_1;

void main()
{
    FragColor = vec4(0.5f, 0.5f, 0.5f, 1.0f);//texture(diffuse_1, TexCoords); // * texture(texture_specular1, TexCoords) * texture(texture_normal1, TexCoords) * texture(texture_height1, TexCoords);
}
