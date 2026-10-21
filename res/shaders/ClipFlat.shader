#shader vertex
#version 330 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_MVP;
uniform float u_PointSize;

void main()
{
    gl_Position = u_MVP * vec4(a_Position, 1.0);
    gl_PointSize = u_PointSize;
}

#shader fragment
#version 330 core

layout(location = 0) out vec4 color;

uniform vec4 u_Color;

void main()
{
    color = u_Color;
}
