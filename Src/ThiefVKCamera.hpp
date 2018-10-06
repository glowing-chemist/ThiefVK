#ifndef THIEFVKCAMERA_HPP
#define THIEFVKCAMERA_HPP

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class ThiefVKCamera {
public:
	ThiefVKCamera(glm::vec3 pos, glm::vec3 dir, float fov) : mPosition{pos}, mDirection{dir}, mFieldOfView{fov} {}

	void rotateLaterally(float degrees);
	void rotateHorizontally(float degrees);

	void translate(glm::vec3 position) { mPosition += position; }

	glm::mat4 getProjectionMatrix() const;
	glm::mat4 getViewMatrix() const;
	glm::vec3 getDirection() const { return mDirection; }

private:

	glm::vec3 mPosition;
	glm::vec3 mDirection;
	float mFieldOfView;
};

#endif