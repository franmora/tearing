#include <string.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <chrono>

#include <GLFW/glfw3.h>

#include "glad/glad.h"
#include "glad/glad_egl.h"

using namespace std;

#if 0
#include <stdio.h>
#include <vector>
#include <string>

#include <GLFW/glfw3.h>

#include "glad/glad.h"

using namespace std;

static const std::string sVertex = R"glsl(
#version 310 es
in vec2 VertexPosition;
void main()
{
	gl_Position = vec4(VertexPosition, 0, 1);
};
)glsl";

static const std::string sFragment = R"glsl(
#version 310 es
precision mediump float;
uniform vec3 FrameColor;
out vec4 finalColor;
void main()
{
	finalColor = vec4(FrameColor, 1);
};
)glsl";

int main()
{
	glfwInit();
	glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	GLFWwindow* glfwWindow = glfwCreateWindow(mode->width, mode->height, "Raspberry PI 4 tearing", nullptr, nullptr);
	glfwMakeContextCurrent(glfwWindow);
	glfwSwapInterval(1);
	gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress);

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	vector<float> sourceGrid = { 0, 0, 1, 0, 0, 1, 1, 1 };
	GLuint sourceBuffer;
	glGenBuffers(1, &sourceBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, sourceBuffer);
	glBufferData(GL_ARRAY_BUFFER, sourceGrid.size() * sizeof(float), sourceGrid.data(), GL_STATIC_DRAW);

	vector<float> targetGrid = { -1, -1, 1, -1, -1, 1, 1, 1 };
	GLuint targetBuffer;
	glGenBuffers(1, &targetBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, targetBuffer);
	glBufferData(GL_ARRAY_BUFFER, targetGrid.size() * sizeof(float), targetGrid.data(), GL_STATIC_DRAW);

	vector<GLushort> indexBuffer = { 0, 1, 2, 3, 1, 2 };
	GLuint indexVertices;
	glGenBuffers(1, &indexVertices);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVertices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indexBuffer.size(), indexBuffer.data(), GL_STATIC_DRAW);

	GLuint targetTexture;
	glGenTextures(1, &targetTexture);
	glBindTexture(GL_TEXTURE_2D, targetTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mode->width, mode->height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

	const GLchar* ShaderSourcePointer;
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	ShaderSourcePointer = sVertex.c_str();
	glShaderSource(VertexShaderID, 1, &ShaderSourcePointer, NULL);
	glCompileShader(VertexShaderID);

	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	ShaderSourcePointer = sFragment.c_str();
	glShaderSource(FragmentShaderID, 1, &ShaderSourcePointer, NULL);
	glCompileShader(FragmentShaderID);

	vector<GLuint> shaderIDs = { VertexShaderID, FragmentShaderID };
	GLuint program = glCreateProgram();
	for (auto& sID : shaderIDs)
	{
		glAttachShader(program, sID);
	}
	glLinkProgram(program);

	GLuint vertexLoc = glGetAttribLocation(program, "VertexPosition");
	GLuint frameColorLoc = glGetUniformLocation(program, "FrameColor");

	glUseProgram(program);

	glEnableVertexAttribArray(vertexLoc);

	glBindBuffer(GL_ARRAY_BUFFER, targetBuffer);
	glVertexAttribPointer(vertexLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	bool toggle = false;
	while (!glfwWindowShouldClose(glfwWindow))
	{
		glfwPollEvents();
		glfwSwapBuffers(glfwWindow);
		glUniform3f(frameColorLoc, toggle ? 1.0 : 0, 0.0, toggle ? 0 : 1.0);
		glDrawElements(GL_TRIANGLES, indexBuffer.size(), GL_UNSIGNED_SHORT, nullptr);
		toggle = !toggle;
	}

	return 0;
}
#else
static const std::string sVertex = R"glsl(
#version 310 es
in vec2 VertexPosition;
in vec2 vertex_UV;
out vec2 UV;
void main()
{
	gl_Position = vec4(VertexPosition, 0, 1);
    UV = vertex_UV;
};
)glsl";

static const std::string sFragment = R"glsl(
#version 310 es
precision mediump float;
in vec2 UV;
uniform sampler2D Texture;
out vec4 finalColor;
void main()
{
	finalColor = texture2D(Texture, UV);
};
)glsl";

static void APIENTRY funcname(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	printf("GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
		(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
		type, severity, message);
}

const unsigned thick = 10;
void DrawLines(unsigned char* imageBuffer, unsigned width, unsigned first, unsigned last, unsigned offset)
{
	for (unsigned row = first; row <= last; row++)
	{
		for (unsigned t = 0; t < thick; t++)
		{
			unsigned pos = offset + t;
			if (pos < width);
			else pos -= width;
			pos = 3 * (row * width + pos);
			imageBuffer[pos + 0] = 0xFF;
			imageBuffer[pos + 1] = 0xFF;
			imageBuffer[pos + 2] = 0xFF;
		}
	}
}

int main()
{
	glfwInit();
	glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	GLFWwindow* glfwWindow = glfwCreateWindow(mode->width, mode->height, "Raspberry PI 4 tearing", nullptr, nullptr);
	glfwMakeContextCurrent(glfwWindow);
	eglGetCurrentDisplay();
	glfwSwapInterval(1);
	gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress);
	gladLoadEGLLoader((GLADloadproc)glfwGetProcAddress);

	glDebugMessageCallback(funcname, nullptr);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glEnable(GL_DEBUG_OUTPUT);

	GLint result = GL_FALSE;
	const GLchar* ShaderSourcePointer;
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	ShaderSourcePointer = sVertex.c_str();
	glShaderSource(VertexShaderID, 1, &ShaderSourcePointer, NULL);
	glCompileShader(VertexShaderID);
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		int InfoLogLength;
		glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
		vector<char> ShaderErrorMessage(InfoLogLength + 1);
		glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &ShaderErrorMessage[0]);
		printf("%s\n", &ShaderErrorMessage[0]);
	}

	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	ShaderSourcePointer = sFragment.c_str();
	glShaderSource(FragmentShaderID, 1, &ShaderSourcePointer, NULL);
	glCompileShader(FragmentShaderID);
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		int InfoLogLength;
		glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
		vector<char> ShaderErrorMessage(InfoLogLength + 1);
		glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &ShaderErrorMessage[0]);
		printf("%s\n", &ShaderErrorMessage[0]);
	}

	vector<GLuint> shaderIDs = { VertexShaderID, FragmentShaderID };
	GLuint program = glCreateProgram();
	for (auto& sID : shaderIDs)
	{
		glAttachShader(program, sID);
	}
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &result);
	if (result == GL_FALSE) {
		int InfoLogLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &InfoLogLength);
		vector<char> ProgramErrorMessage(InfoLogLength + 1);
		glGetProgramInfoLog(program, InfoLogLength, NULL, &ProgramErrorMessage[0]);
		printf("%s\n", &ProgramErrorMessage[0]);
	}

	GLuint uvLoc = glGetAttribLocation(program, "vertex_UV");
	GLuint vertexLoc = glGetAttribLocation(program, "VertexPosition");

	glUseProgram(program);

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	vector<float> sourceGrid = { 0, 0, 1, 0, 0, 1, 1, 1 };
	GLuint sourceBuffer;
	glGenBuffers(1, &sourceBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, sourceBuffer);
	glBufferData(GL_ARRAY_BUFFER, sourceGrid.size() * sizeof(float), sourceGrid.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(uvLoc);
	glVertexAttribPointer(uvLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	vector<float> targetGrid = { -1, -1, 1, -1, -1, 1, 1, 1 };
	GLuint targetBuffer;
	glGenBuffers(1, &targetBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, targetBuffer);
	glBufferData(GL_ARRAY_BUFFER, targetGrid.size() * sizeof(float), targetGrid.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(vertexLoc);
	glVertexAttribPointer(vertexLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	vector<GLushort> indexBuffer = { 0, 1, 2, 3, 1, 2 };
	GLuint indexVertices;
	glGenBuffers(1, &indexVertices);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVertices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indexBuffer.size(), indexBuffer.data(), GL_STATIC_DRAW);

	GLuint targetTexture;
	glGenTextures(1, &targetTexture);
	glBindTexture(GL_TEXTURE_2D, targetTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mode->width, mode->height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

	GLuint sourceTexture;
	glGenTextures(1, &sourceTexture);
	glBindTexture(GL_TEXTURE_2D, sourceTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	const unsigned width = 1280;
	const unsigned height = 720;
	unsigned char* imageBuffer = new unsigned char[width * height * 3];

	unsigned c = 0;
	while (!glfwWindowShouldClose(glfwWindow))
	{
		memset(imageBuffer, 0x00, width * height * 3);
		DrawLines(imageBuffer, width, 0, height / 3 - 1, c / 4);
		DrawLines(imageBuffer, width, height / 3 + 1, 2 * height / 3 - 1, c / 2);
		DrawLines(imageBuffer, width, 2 * height / 3 + 1, height - 1, c);
		c += 8;
		if (c > width) c = 0;
		glfwPollEvents();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageBuffer);
		glDrawElements(GL_TRIANGLES, indexBuffer.size(), GL_UNSIGNED_SHORT, nullptr);
		glfwSwapBuffers(glfwWindow);
	}

	delete imageBuffer;

	return 0;
}
#endif
