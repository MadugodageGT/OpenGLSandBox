#shader vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;

uniform mat4 u_MVP;

out vec3 v_Position;
out vec3 v_Normal;

void main()
{
    v_Position = a_Position;
    v_Normal = a_Normal;
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}

#shader fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec3 v_Position;
in vec3 v_Normal;

uniform vec4 u_CamPos;
uniform vec4 u_Color;
uniform float u_Alpha;

void main()
{
    // Headlight, two-sided. The same program draws the model and the annotation tint, so the depth
    // of both passes is bit-identical and the second pass can safely use GL_LEQUAL.
    vec3 n = normalize(v_Normal);
    vec3 l = normalize(u_CamPos.xyz - v_Position);
    float diffuse = abs(dot(n, l));
    color = vec4(u_Color.rgb * (0.25 + 0.75 * diffuse), u_Alpha);
}
