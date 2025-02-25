#version 460 core
layout (location = 0) in vec4 aPos; // last float is uv.x :)
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec4 aNormal; // last float is uv.y
layout (location = 3) in uvec4 aBoneNum;
layout (location = 4) in vec4 aBoneWeight;

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 normal;
layout (location = 2) out vec2 texCoord;

struct MorphVertex {
  vec4 position;
  vec4 normal;
};

layout (std140, binding = 0) uniform Matrices {
  mat4 view;
  mat4 projection;
};

layout (std430, binding = 1) readonly restrict buffer BoneMatrices {
  mat4 boneMat[];
};

layout (std430, binding = 2) readonly restrict buffer WorldPosMatrices {
  mat4 worldPos[];
};

layout (std430, binding = 3) readonly restrict buffer InstanceSelected {
  vec2 selected[];
};

layout (std430, binding = 4) readonly restrict buffer AnimMorphBuffer {
  MorphVertex morphVertices[];
};

layout (std430, binding = 5) readonly restrict buffer AnimMorphData {
  vec4 vertsPerMorphAnim[];
};

uniform int aModelStride;

void main() {

  int modelStride = gl_InstanceID * aModelStride;

  mat4 skinMat =
    aBoneWeight.x * boneMat[aBoneNum.x + modelStride] +
    aBoneWeight.y * boneMat[aBoneNum.y + modelStride] +
    aBoneWeight.z * boneMat[aBoneNum.z + modelStride] +
    aBoneWeight.w * boneMat[aBoneNum.w + modelStride];

  mat4 worldPosSkinMat = worldPos[gl_InstanceID] * skinMat;

  /* y and z data contain the offset into the morph anim buffer */
  int morphAnimIndex = int(vertsPerMorphAnim[gl_InstanceID].y * vertsPerMorphAnim[gl_InstanceID].z);

  vec4 origVertex = vec4(aPos.x, aPos.y, aPos.z, 1.0);
  vec4 morphVertex = vec4(morphVertices[gl_VertexID + morphAnimIndex].position.xyz, 1.0);

  gl_Position = projection * view * worldPosSkinMat * mix(origVertex, morphVertex, vertsPerMorphAnim[gl_InstanceID].x);

  color = aColor * selected[gl_InstanceID].x;
  /* draw the instance always on top when highlighted, helps to find it better */
  if (selected[gl_InstanceID].x != 1.0f) {
    gl_Position.z -= 1.0f;
  }

  vec4 origNormal = vec4(aNormal.x, aNormal.y, aNormal.z, 1.0);
  vec4 morphNormal = vec4(morphVertices[gl_VertexID + morphAnimIndex].normal.xyz, 1.0);
  normal = transpose(inverse(worldPosSkinMat)) * mix(origNormal, morphNormal, vertsPerMorphAnim[gl_InstanceID].x);

  texCoord = vec2(aPos.w, aNormal.w);
}
