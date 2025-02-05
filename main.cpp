#include <vector>
#include <stdio.h>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gl/glew.h>
#include <GLFW/glfw3.h>

int main(int argc, char** argv)
{
	glfwSetErrorCallback([](int error, const char* description) {
		std::cerr << "Error: " << description << std::endl;
		});

	if (!glfwInit())
	{
		std::cerr << "Unable to initialize GLFW" << std::endl;
		exit(EXIT_FAILURE);
	}
	else
		std::cout << "> GLFW Initialization" << std::endl;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	GLFWwindow* window = glfwCreateWindow(800, 600, "project-hibernation", NULL, NULL);
	if (!window)
	{
		std::cerr << "Unable to create window" << std::endl;
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	else
		std::cout << "> Window Creation" << std::endl;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		std::cerr << "Unable to initialize GLEW" << std::endl;
		exit(EXIT_FAILURE);
	}
	else
		std::cout << "> GLEW Initialization" << std::endl;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	while (!glfwWindowShouldClose(window))
	{
		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}