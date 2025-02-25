#include "SkyboxBuffer.h"
#include "Logger.h"

void SkyboxBuffer::init() {
  glGenVertexArrays(1, &mVAO);
  glGenBuffers(1, &mVertexVBO);

  glBindVertexArray(mVAO);

  glBindBuffer(GL_ARRAY_BUFFER, mVertexVBO);

  /* we need only position data */
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(OGLSkyboxVertex), (void*) offsetof(OGLSkyboxVertex, position));

  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  Logger::log(1, "%s: VAO and VBOs initialized\n", __FUNCTION__);
}

void SkyboxBuffer::cleanup() {
  glDeleteBuffers(1, &mVertexVBO);
  glDeleteVertexArrays(1, &mVAO);
}

void SkyboxBuffer::uploadData(std::vector<OGLSkyboxVertex> vertexData) {
  if (vertexData.empty()) {
    Logger::log(1, "%s error: invalid data to upload\n", __FUNCTION__, vertexData.size());
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, mVertexVBO);
  glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(OGLSkyboxVertex), &vertexData.at(0), GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  mNumVertices = vertexData.size();
}

void SkyboxBuffer::bind() {
  glBindVertexArray(mVAO);
}

void SkyboxBuffer::unbind() {
  glBindVertexArray(0);
}

void SkyboxBuffer::draw() {
  /* change depth test for skybox  */
  glDisable(GL_DEPTH_TEST);
  glDrawArrays(GL_TRIANGLES, 0, mNumVertices);
  glEnable(GL_DEPTH_TEST);
}

void SkyboxBuffer::bindAndDraw() {
  bind();
  draw();
  unbind();
}
