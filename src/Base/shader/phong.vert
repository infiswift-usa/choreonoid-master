#version 330

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec3 vertexNormal;

out vec3 position;
out vec3 normal;

/*
  Uniform blocks make rendering slow in the Ubuntu platforms with Intel GPUs.
  If the following definition is disabled and the definition of usual uniform
  variables are enabled instead of it, GLSLSceneRenderer uses the latter one
  to pass the matrix values. For the Intel GPUs, this results in better performace.
*/
layout(std140) uniform TransformBlock {
    mat4 modelViewMatrix;
    mat4 MVP;
    mat3 normalMatrix;
};

/*
uniform mat4 modelViewMatrix;
uniform mat3 normalMatrix;
uniform mat4 MVP;
*/

void main()
{
    normal = normalize(normalMatrix * vertexNormal);
    position = vec3(modelViewMatrix * vec4(vertexPosition, 1.0));

    gl_Position = MVP * vec4(vertexPosition, 1.0);
}
