#include <iostream>
#include <thread>
#include <cmath>
#include <stdio.h>
#include <format>
#include <string>
#include <Windows.h>
#include "imports/serial/ceSerial.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
#include <GLFW/glfw3.h>

#define M_PI 3.14159265358979323846
#define READ_STEPS 0
#define READ_DIST 1
#define READ_MOVE 2

using namespace std;

void serialLoop(GLFWwindow* window);
bool render(GLFWwindow* window);
ImVec2 calcPoint(int step, int dist);

//struct Ping { //contains the number of steps around and the distance of the ping
//	int step = 0; //0-2048
//	int distance = 0; //cm
//};

//Data gained from sensor. Serial thread writes to pings while render thread reads
atomic_int pings[2048]; //distances
atomic_int recent; //most recently updated ping
int moveSize = 16;
//ImVec2 points[2048]; //GUI points, currently calculated on the fly in render function

ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main() {
	cout << "Program starting" << endl;

	cout << "Setting up GUI" << endl;

	//Set up glfw
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		return 1;
	}
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL2 example", NULL, NULL);
	if (window == NULL) {
		return 1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL2_Init();

	//create threads
	thread serialThread(serialLoop, window);
	
	//main render loop
	while (!glfwWindowShouldClose(window)) {
		render(window);
	}

	//wait for threads to finish when user closes window
	serialThread.join();
	
	//close gui
	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

void serialLoop(GLFWwindow* window) {
	//Serial Port
	cout << "Opening serial port" << endl;
	ce::ceSerial com("\\\\.\\COM5", 9600, 8, 'N', 1);
	cout << "Port: " << com.GetPort().c_str() << endl;
	if (com.Open() == 0) {
		cout << "Opened successfully" << endl;
	}
	else {
		cout << "Problem opening port" << endl;
	}

	cout << "Reading..." << endl;

	//main loop
	bool successFlag;
	string steps;
	string dist;
	string move;
	int state = READ_STEPS;
	//main loop
	while (com.ReadChar(successFlag) != '\n');

	while (!glfwWindowShouldClose(window)) {
		char c = com.ReadChar(successFlag); //read one character
		assert(isdigit(c) || c == ',' || c == '\n' || c == '\r');
		if (successFlag) { //recieved a char
			if (c == '\r') {
				continue;
			}
			if (isdigit(c)) {
				if (state == READ_STEPS) {
					steps += c;
				}
				else if (state == READ_DIST) {
					dist += c;
				}
				else { //state == READ_MOVE
					move += c;
				}
			}
			else if (c == ',') {
				state++;
			}
			else if (c == '\n') {
				pings[stoi(steps)] = stoi(dist);
				recent = stoi(steps);
				steps.clear();
				dist.clear();
				move.clear();
				state = READ_STEPS;
			}
		}
	}
	//close serial
	cout << "Closing serial port";
	com.Close();
}

bool render(GLFWwindow *window) {
	glfwPollEvents();

	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Radar");
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	ImVec2 p1 = ImGui::GetCursorScreenPos();
	ImVec2 windowMax = ImGui::GetWindowContentRegionMax();
	ImVec2 windowMin = ImGui::GetWindowContentRegionMin();
	ImVec2 windowSize = ImVec2(windowMax.x - windowMin.x, windowMax.y - windowMin.y);

	ImVec2 middle = ImVec2(p1.x + windowSize.x/2, p1.y + windowSize.y/2);

	ImU32 col = ImColor(192, 0, 0, 255);

	//head line
	ImVec2 head = calcPoint(recent, 400);
	ImVec2 windowHead = ImVec2(middle.x + head.x, middle.y + head.y);
	draw_list->AddLine(middle, windowHead, col, 1.0);

	col = ImColor(50, 205, 50, 255);

	//draw map
	for (int i = 0; i < sizeof(pings)/sizeof(pings[0]); i++) {
		ImVec2 pos0 = calcPoint(i - moveSize, pings[i - moveSize]);
		ImVec2 windowPos0 = ImVec2(middle.x + pos0.x, middle.y + pos0.y);
		ImVec2 pos1 = calcPoint(i, pings[i]);
		ImVec2 windowPos1 = ImVec2(middle.x + pos1.x, middle.y + pos1.y);
		draw_list->AddLine(windowPos0, windowPos1, col, 1.0);
	}

	ImGui::End();

	ImGui::Begin("Device Input");
	string text = format("Step: {} Distance: {} cm", (int)recent, (int)pings[recent]);
	ImGui::Text(text.c_str());

	ImGui::End();

	ImGui::Render();
	int display_w, display_h;
	glfwGetFramebufferSize(window, &display_w, &display_h);
	glViewport(0, 0, display_w, display_h);
	glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
	glClear(GL_COLOR_BUFFER_BIT);

	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

	glfwMakeContextCurrent(window);
	glfwSwapBuffers(window);
	return true;
}

//takes steps around and distance measured by sonar and returns x and y values using sin/cos
ImVec2 calcPoint(int step, int dist) {
	int x = dist * cos(((float)step) / 2048 * 2 * M_PI);
	int y = dist * sin(((float)step) / 2048 * 2 * M_PI);
	return ImVec2(x, y);
}