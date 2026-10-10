#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>


#include "Renderer.h"
#include "VertexBuffer.h"
#include "VertexBufferLayout.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Texture.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp" 

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "tests/Test.h"
#include "tests/TestClearColor.h"
#include "tests/TestQuadColor.h"
#include "tests/TestTexture2D.h"
#include "tests/TestGoing3D.h"
#include "tests/TestBatchRendering.h"
#include "tests/TestIsoMetric.h"
#include "tests/TestLoadingMesh.h"
#include "tests/TestParticleSystem.h"
#include "tests/TestStencilAnnotation.h"

void KeyCallback(
    GLFWwindow* window,
    int key,
    int scancode,
    int action,
    int mods
);

void MouseButtonCallback(
    GLFWwindow* window,
    int button,
    int action,
    int mods);

int main(void)
{
    GLFWwindow* window;

    if (!glfwInit())
        return -1;


    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(1024, 720, "OpenGL SandBox", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);

    if (glewInit() != GLEW_OK)
        std::cout << "Error!"<< std::endl;



   GLCall(glEnable(GL_BLEND));   
   GLCall(glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));



    ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, false);
	ImGui_ImplOpenGL3_Init("#version 330");

    test::Test* currentTest = nullptr;
    test::TestMenu* testMenu = new test::TestMenu(currentTest);
    currentTest = testMenu;

    glfwSetWindowUserPointer(window, &currentTest);
    glfwSetKeyCallback(window, KeyCallback);
	glfwSetMouseButtonCallback(window, MouseButtonCallback);

    //testMenu->RegisterTest<test::TestClearColor>("Clear Color");
    //testMenu->RegisterTest<test::TestQuadColor>("Quad Color");
    //testMenu->RegisterTest<test::TestGoing3D>("Going 3D");
    //testMenu->RegisterTest<test::TestBatchRendering>("Batch Rendering");
    //testMenu->RegisterTest<test::TestIsoMetric>("IsoMetric");
    testMenu->RegisterTest<test::TestLoadingMesh>("Loadin Mesh");
    testMenu->RegisterTest<test::TestStencilAnnotation>("Stencil Annotation");
	//testMenu->RegisterTest<test::TestParticleSystem>("Particle System");


    while (!glfwWindowShouldClose(window))
    {

        glfwPollEvents();

        GLCall(glClearColor(0.0f, 0.0f, 0.0f, 0.0f););
        GLCall(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));


        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (currentTest)
        {
            currentTest->OnUpdate(0.0f);
            currentTest->OnRender();
            ImGui::Begin("Test");
            if (currentTest != testMenu && ImGui::Button("<-"))
            {
                delete currentTest;
                currentTest = testMenu;
            }
            currentTest->OnImGuiRender();
            ImGui::End();

        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        

        glfwSwapBuffers(window);

    }
    delete currentTest;
    if (currentTest != testMenu)
        delete testMenu;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();

    return 0;
}




void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    test::Test** currentTest = static_cast<test::Test**>(glfwGetWindowUserPointer(window));

    if (*currentTest)
    {
        (*currentTest)->OnEvent(key, action);
    }
}


void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{

    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    test::Test** currentTest =
        static_cast<test::Test**>(glfwGetWindowUserPointer(window));


    if (*currentTest)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        (*currentTest)->OnMouseButtonEvent(
            button,
            action,
            mouseX,
            mouseY
        );
    }
}