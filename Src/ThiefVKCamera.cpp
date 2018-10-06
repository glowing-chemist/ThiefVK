#include "ThiefVKCamera.hpp"

#include <glm/gtx/vector_angle.hpp>


void ThiefVKCamera::rotateLaterally(float degrees) {
	glm::mat4 rotationMat = glm::rotate(glm::mat4(1.0f), glm::radians(degrees), glm::vec3(0.0f, 1.0f, 0.0f));
	mDirection = mDirection * glm::mat3(rotationMat);
}


void ThiefVKCamera::rotateHorizontally(float degrees) {
	glm::mat4 rotationMat = glm::rotate(glm::mat4(1.0f), glm::radians(degrees), glm::vec3(1.0f, 0.0f, 0.0f));
	mDirection = mDirection * glm::mat3(rotationMat);
}


glm::mat4 ThiefVKCamera::getProjectionMatrix() const {
	return glm::perspective(glm::radians(mFieldOfView), 1.0f, 0.1f, 10.0f);
}


glm::mat4 ThiefVKCamera::getViewMatrix() const {
	return glm::lookAt(mPosition, mPosition + mDirection, glm::vec3(0.0f, 1.0f, 0.0f));
}