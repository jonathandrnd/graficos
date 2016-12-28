#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>

//#include <GL/gl.h>
//#include <GL/glu.h>

#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include <FTGL/ftgl.h>
#include <SOIL/SOIL.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ao/ao.h>
#include <mpg123.h>
#include <unistd.h>
#include <signal.h>

#define GAME_BIRD 0
#define GAME_WOOD_VERTICAL 1
#define GAME_WOOD_HORIZONTAL 1
#define GAME_PIG 2
#define GAME_SCOREBOARD 3

#define BITS 8

pid_t pid;

using namespace std;
void reshapeWindow (GLFWwindow* window, int width, int height);

class VAO {
	public:
		GLuint VertexArrayID;
		GLuint VertexBuffer;
		GLuint ColorBuffer;
		GLuint TextureBuffer;
		GLuint TextureID;

		GLenum PrimitiveMode;
		GLenum FillMode;
		int NumVertices;
		double centerx;
		double centery;
		double radius;
		double mass;
		int type;
		int dead;

		VAO(){
		}
};
//typedef struct VAO VAO;

struct GLMatrices {
	glm::mat4 projection;
	glm::mat4 model;
	glm::mat4 view;
	GLuint MatrixID;
	GLuint TexMatrixID; // For use with texture shader
} Matrices;

struct FTGLFont {
	FTFont* font;
	GLuint fontMatrixID;
	GLuint fontColorID;
} GL3Font;

GLuint programID, fontProgramID, textureProgramID;;

/* Function to load Shaders - Use it as it is */
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path) {

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open())
	{
		std::string Line = "";
		while(getline(VertexShaderStream, Line))
			VertexShaderCode += "\n" + Line;
		VertexShaderStream.close();
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
		std::string Line = "";
		while(getline(FragmentShaderStream, Line))
			FragmentShaderCode += "\n" + Line;
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> VertexShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &VertexShaderErrorMessage[0]);

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> FragmentShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
	fprintf(stdout, "%s\n", &FragmentShaderErrorMessage[0]);

	// Link the program
	fprintf(stdout, "Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> ProgramErrorMessage( max(InfoLogLength, int(1)) );
	glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
	fprintf(stdout, "%s\n", &ProgramErrorMessage[0]);

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

void quit(GLFWwindow *window)
{
	glfwDestroyWindow(window);
	glfwTerminate();
	kill(pid,SIGKILL);
	exit(EXIT_SUCCESS);
}

glm::vec3 getRGBfromHue (int hue)
{
	float intp;
	float fracp = modff(hue/60.0, &intp);
	float x = 1.0 - abs((float)((int)intp%2)+fracp-1.0);

	if (hue < 60)
		return glm::vec3(1,x,0);
	else if (hue < 120)
		return glm::vec3(x,1,0);
	else if (hue < 180)
		return glm::vec3(0,1,x);
	else if (hue < 240)
		return glm::vec3(0,x,1);
	else if (hue < 300)
		return glm::vec3(x,0,1);
	else
		return glm::vec3(1,0,x);
}


/* Generate VAO, VBOs and return VAO handle */
VAO* create3DObject (GLenum primitive_mode, int numVertices, int type, const GLfloat* vertex_buffer_data, const GLfloat* color_buffer_data, double centerx, double centery, double radius, GLenum fill_mode )
{
	VAO* vao = new  VAO();
	vao->PrimitiveMode = primitive_mode;
	vao->NumVertices = numVertices;
	vao->FillMode = fill_mode;

	vao->centerx = centerx;
	vao->centery = centery;
	vao->radius = radius;
	vao->type = type;
	vao->dead = 0;

	// Create Vertex Array Object
	// Should be done after CreateWindow and before any other GL calls
	glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
	glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
	glGenBuffers (1, &(vao->ColorBuffer));  // VBO - colors

	glBindVertexArray (vao->VertexArrayID); // Bind the VAO 
	glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices 
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
	glVertexAttribPointer(
			0,                  // attribute 0. Vertices
			3,                  // size (x,y,z)
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);

	glBindBuffer (GL_ARRAY_BUFFER, vao->ColorBuffer); // Bind the VBO colors 
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), color_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
	glVertexAttribPointer(
			1,                  // attribute 1. Color
			3,                  // size (r,g,b)
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
			);

	return vao;
}

/* Generate VAO, VBOs and return VAO handle - Common Color for all vertices */
VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat red, const GLfloat green, const GLfloat blue, double centerx, double centery, double radius, GLenum fill_mode)
{
	GLfloat* color_buffer_data = new GLfloat [3*numVertices];
	for (int i=0; i<numVertices; i++) {
		color_buffer_data [3*i] = red;
		color_buffer_data [3*i + 1] = green;
		color_buffer_data [3*i + 2] = blue;
	}

	return create3DObject(primitive_mode, numVertices, 10, vertex_buffer_data, color_buffer_data, centerx, centery, radius, fill_mode);
}


struct VAO* create3DTexturedObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* texture_buffer_data, GLuint textureID, GLenum fill_mode=GL_FILL)
{
	struct VAO* vao = new struct VAO;
	vao->PrimitiveMode = primitive_mode;
	vao->NumVertices = numVertices;
	vao->FillMode = fill_mode;
	vao->TextureID = textureID;

	// Create Vertex Array Object
	// Should be done after CreateWindow and before any other GL calls
	glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
	glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
	glGenBuffers (1, &(vao->TextureBuffer));  // VBO - textures

	glBindVertexArray (vao->VertexArrayID); // Bind the VAO
	glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices
	glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
	glVertexAttribPointer(
						  0,                  // attribute 0. Vertices
						  3,                  // size (x,y,z)
						  GL_FLOAT,           // type
						  GL_FALSE,           // normalized?
						  0,                  // stride
						  (void*)0            // array buffer offset
						  );

	glBindBuffer (GL_ARRAY_BUFFER, vao->TextureBuffer); // Bind the VBO textures
	glBufferData (GL_ARRAY_BUFFER, 2*numVertices*sizeof(GLfloat), texture_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
	glVertexAttribPointer(
						  2,                  // attribute 2. Textures
						  2,                  // size (s,t)
						  GL_FLOAT,           // type
						  GL_FALSE,           // normalized?
						  0,                  // stride
						  (void*)0            // array buffer offset
						  );

	return vao;
}

/* Render the VBOs handled by VAO */
void draw3DObject (VAO* vao)
{
	// Change the Fill Mode for this object
	glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

	// Bind the VAO to use
	glBindVertexArray (vao->VertexArrayID);

	// Enable Vertex Attribute 0 - 3d Vertices
	glEnableVertexAttribArray(0);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

	// Enable Vertex Attribute 1 - Color
	glEnableVertexAttribArray(1);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->ColorBuffer);

	// Draw the geometry !
	glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle
}

void draw3DTexturedObject (struct VAO* vao)
{
	// Change the Fill Mode for this object
	glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

	// Bind the VAO to use
	glBindVertexArray (vao->VertexArrayID);

	// Enable Vertex Attribute 0 - 3d Vertices
	glEnableVertexAttribArray(0);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

	// Bind Textures using texture units
	glBindTexture(GL_TEXTURE_2D, vao->TextureID);

	// Enable Vertex Attribute 2 - Texture
	glEnableVertexAttribArray(2);
	// Bind the VBO to use
	glBindBuffer(GL_ARRAY_BUFFER, vao->TextureBuffer);

	// Draw the geometry !
	glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle

	// Unbind Textures to be safe
	glBindTexture(GL_TEXTURE_2D, 0);
}

/* Create an OpenGL Texture from an image */
GLuint createTexture (const char* filename)
{
	GLuint TextureID;
	// Generate Texture Buffer
	glGenTextures(1, &TextureID);
	// All upcoming GL_TEXTURE_2D operations now have effect on our texture buffer
	glBindTexture(GL_TEXTURE_2D, TextureID);
	// Set our texture parameters
	// Set texture wrapping to GL_REPEAT
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Set texture filtering (interpolation)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Load image and create OpenGL texture
	int twidth, theight;
	unsigned char* image = SOIL_load_image(filename, &twidth, &theight, 0, SOIL_LOAD_RGB);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, twidth, theight, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D); // Generate MipMaps to use
	SOIL_free_image_data(image); // Free the data read from file after creating opengl texture
	glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture when done, so we won't accidentily mess it up

	return TextureID;
}

/**************************
 * Customizable functions *
 **************************/

int pressed_state = 0, collision_state=0, zoominstate = 0, zoomoutstate = 0, panright = 0, panleft = 0, panup = 0, pandown = 0;
int keyboard_pressed_statex = 0, keyboard_pressed_statey = 0;
double curx,cury,initx = -380,inity = 130,speedx,speedy,strength=0.5,prevx,prevy,cannonball_size=18,gravity=0.2;
double fireposx=-380,fireposy=130, keyboardx = -380 , keyboardy = 130;
double pivotx=-10,pivoty=-30,angular_v[6],angle[6],woodspx[6],woodspy[6],pigspx[10], pigspy[10], piginitx[10];
VAO  *cannonball, *gameFloor, *woodlogs[6], *pigs[10], *powerboard, *powerelement, *background, *catapult;
float screenleft = -600.0f, screenright = 600.0f, screentop = -300.0f, screenbotton = 300.0f;
int scoretimer[10][3],tim=5;
int pig_wood[10];
int panning_state=0, paninitx, paninity;

int score = 0;

double power = 0;
/* Executed when a regular key is pressed/released/held-down */
/* Prefered for Keyboard events */
void keyboard (GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Function is called first on GLFW_PRESS.

	if (action == GLFW_RELEASE) {
		switch (key) {
			case GLFW_KEY_C:
				//rectangle_rot_status = !rectangle_rot_status;
				break;
			case GLFW_KEY_KP_ADD:
				zoominstate = 0;
				//triangle_rot_status = !triangle_rot_status;
				break;
			case GLFW_KEY_KP_SUBTRACT:
				zoomoutstate = 0;
				break;
			case GLFW_KEY_LEFT:
				panleft = 0;
				break;
			case GLFW_KEY_RIGHT:
				panright = 0;
				break;
			case GLFW_KEY_UP:
				panup = 0;
				break;
			case GLFW_KEY_DOWN:
				pandown = 0;
				break;
			case GLFW_KEY_X:
				// do something ..
				break;
			case GLFW_KEY_A:
				keyboard_pressed_statex = 0;
				break;
			case GLFW_KEY_B:
				keyboard_pressed_statey = 0;
				break;
			default:
				break;
		}
	}
	else if (action == GLFW_PRESS) {
		switch (key) {
			case GLFW_KEY_ESCAPE:
				quit(window);
				break;
			case GLFW_KEY_KP_ADD:
				zoominstate=1;
				break;
			case GLFW_KEY_KP_SUBTRACT:
				zoomoutstate = 1;
				break;
			case GLFW_KEY_LEFT:
				panleft = 1;
				break;
			case GLFW_KEY_RIGHT:
				panright = 1;
				break;
			case GLFW_KEY_UP:
				panup = 1;
				break;
			case GLFW_KEY_DOWN:
				pandown = 1;
				break;
			case GLFW_KEY_A:
				keyboard_pressed_statex = 1;
				if(keyboard_pressed_statey==0){
					keyboardx = fireposx;
					keyboardy = fireposy;
					initx = fireposx;
					inity = fireposy;
				}
				break;
			case GLFW_KEY_B:
				keyboard_pressed_statey = 1;
				if(keyboard_pressed_statex==0){
					keyboardy = fireposy;
					keyboardx = fireposx;
					initx = fireposx;
					inity = fireposy;
				}
				break;
			case GLFW_KEY_SPACE:
				pressed_state = 3;
				if(sqrt((keyboardx-initx)*(keyboardx-initx)+(keyboardy-inity)*(keyboardy-inity)) > 30){
					double angle_present = -M_PI+atan2(inity-keyboardy,initx-keyboardx);
					keyboardx = initx + 30*cos(angle_present);
					keyboardy = inity + 30*sin(angle_present);
				}
				speedx=(initx-keyboardx)*strength;
				speedy=(inity-keyboardy)*strength;
				break;

			default:
				break;
		}
	}
}

/* Executed for character input (like in text boxes) */
void keyboardChar (GLFWwindow* window, unsigned int key)
{
	switch (key) {
		case 'Q':
		case 'q':
			quit(window);
			break;
		default:
			break;
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	if(yoffset == 1){
		screenleft /= 1.02;
		screenright /= 1.02;
		screentop /= 1.02;
		screenbotton /= 1.02;
		reshapeWindow(window, 1200, 600);
	}
	else if(yoffset == -1){
		if(screenleft >= -600.0f/1.02f)
			screenleft *= 1.02;
		if(screenright <= 600.0f/1.02f)
			screenright *= 1.02;
		if(screentop >= -300.0f/1.02f)
			screentop *= 1.02;
		if(screenbotton <= 300.0f/1.02f)
			screenbotton *= 1.02;
		reshapeWindow(window, 1200, 600);
	}

}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	curx = ((screenright - screenleft)/1200.0f)*xpos + screenleft;
	cury = ((screenbotton - screentop)/600.0f)*ypos + screentop;
	if(panning_state == 1){
		if(paninitx - curx < 0 && screenleft >= -600 + fabs(paninitx - curx)){
			screenleft -= fabs(paninitx - curx);
			screenright -= fabs(paninitx - curx);
		}
		if(paninitx - curx > 0 && screenright <= 600 - fabs(paninitx -curx)){
			screenleft += fabs(paninitx -curx);
			screenright += fabs(paninitx -curx);
		}
		if(paninity - cury < 0  && screentop >= -300 + fabs(paninity - cury)){
			screentop -= fabs(paninity - cury);
			screenbotton -= fabs(paninity - cury);
		}
		if(paninity - cury > 0 && screenbotton <= 300 - fabs(paninity -cury)){
			screentop += fabs(paninity - cury);
			screenbotton += fabs(paninity -cury);
		}
		reshapeWindow(window, 1200, 600);
	}

}

int is_cannon_clicked(double mousex, double mousey) {
	double x2=cannonball->centerx;
	double y2=cannonball->centery;
	double radius = cannonball->radius;
	if(radius>sqrt((x2-mousex)*(x2-mousex)+(y2-mousey)*(y2-mousey)))
		return true;
	else
		return false;
}

/* Executed when a mouse button is pressed/released */
void mouseButton (GLFWwindow* window, int button, int action, int mods)
{
	switch (button) {
		case GLFW_MOUSE_BUTTON_LEFT:
			if (action == GLFW_PRESS){
				//triangle_rot_dir *= -1;
				if(pressed_state==0&&is_cannon_clicked(curx,cury))
				{	
					initx=curx,inity=cury;
					pressed_state=1;
				}
			}
			if(pressed_state==3){
				pressed_state=0;
				gravity = 0.2;
				power = 0;
				keyboardx = fireposx;
				keyboardy = fireposy;
			}
			if (action == GLFW_RELEASE ){
				//triangle_rot_dir *= -1;
				if(pressed_state==1){
					pressed_state=3;

					if(sqrt((curx-initx)*(curx-initx)+(cury-inity)*(cury-inity)) > 30){
						double angle_present = -M_PI+atan2(inity-cury,initx-curx);
						curx = initx + 30*cos(angle_present);
						cury = inity + 30*sin(angle_present);
					}
					speedx=(initx-curx)*strength;
					speedy=(inity-cury)*strength;

				}
				//printf("pressed at %lf %lf and released at %lf %lf",initx,inity,curx,cury);
			}
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			if (action == GLFW_RELEASE) {
				panning_state = 0;
			}
			if(action == GLFW_PRESS){
				panning_state = 1;
				paninitx = curx;
				paninity = cury;
			}
			break;
		default:
			break;
	}
}


/* Executed when window is resized to 'width' and 'height' */
/* Modify the bounds of the screen here in glm::ortho or Field of View in glm::Perspective */
void reshapeWindow (GLFWwindow* window, int width, int height)
{
	int fbwidth=width, fbheight=height;
	/* With Retina display on Mac OS X, GLFW's FramebufferSize
	   is different from WindowSize */
	glfwGetFramebufferSize(window, &fbwidth, &fbheight);

	GLfloat fov = 90.0f;

	// sets the viewport of openGL renderer
	glViewport (0, 0, (GLsizei) fbwidth, (GLsizei) fbheight);

	// set the projection matrix as perspective
	/* glMatrixMode (GL_PROJECTION);
	   glLoadIdentity ();
	   gluPerspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1, 500.0); */
	// Store the projection matrix in a variable for future use
	// Perspective projection for 3D views
	// Matrices.projection = glm::perspective (fov, (GLfloat) fbwidth / (GLfloat) fbheight, 0.1f, 500.0f);

	// Ortho projection for 2D views
//	screenleft = -screenleft, screenright = -screenright, screenbotton= - screenbotton, screentop = -screentop;
	Matrices.projection = glm::ortho(screenleft, screenright, screenbotton, screentop, 1.0f, 500.0f);
}

void createPowerElement() {
	static GLfloat vertex_buffer_data[] = {
		-0.5, -15.0f*0.75, 0,
		0.5, -15.0f*0.75f, 0,
		-0.5, 15.0f*0.75f, 0,

		0.5, -15.0f*0.75f, 0,
		-0.5, 15.0f*0.75f, 0,
		0.5, 15.0f*0.75f, 0
	};
	static GLfloat color_buffer_data[] = {
		12.0f/255.0f,253.0f/255.0f,1.0f/255.0f,
		12.0f/255.0f,253.0f/255.0f,1.0f/255.0f,
		12.0f/255.0f,253.0f/255.0f,1.0f/255.0f,
		12.0f/255.0f,253.0f/255.0f,1.0f/255.0f,
		12.0f/255.0f,253.0f/255.0f,1.0f/255.0f,
		12.0f/255.0f,253.0f/255.0f,1.0f/255.0f
	};
	powerelement = create3DObject(GL_TRIANGLES, 2*3 ,GAME_SCOREBOARD, vertex_buffer_data, color_buffer_data, -400 + 30, -270 +15, 30, GL_FILL);
}

void createPowerBoard(){
	double leftoffset = -280;
	double width = 260;
	double topoffset = -240;
	double height = 15;
	double innerratio = 0.83;

	static  GLfloat vertex_buffer_data[] = {
		leftoffset - width, topoffset - height, 0,
		leftoffset + width, topoffset - height, 0,
		leftoffset - width, topoffset + height, 0,

		leftoffset + width, topoffset - height, 0,
		leftoffset - width, topoffset + height, 0,
		leftoffset + width, topoffset + height, 0,

		leftoffset - width * innerratio, topoffset - height * innerratio, 0,
		leftoffset + width * innerratio, topoffset - height * innerratio, 0,
		leftoffset - width * innerratio, topoffset + height * innerratio, 0,

		leftoffset + width * innerratio, topoffset - height * innerratio, 0,
		leftoffset - width * innerratio, topoffset + height * innerratio, 0,
		leftoffset + width * innerratio, topoffset + height * innerratio, 0,
		
	};
	static GLfloat color_buffer_data[] = {
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f,
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f,
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f,
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f,
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f,
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f,
		
		123.0f/255.0f,187.0f/255.0f,70.0f/255.0f,
		123.0f/255.0f,187.0f/255.0f,70.0f/255.0f,
		123.0f/255.0f,187.0f/255.0f,70.0f/255.0f,
		123.0f/255.0f,187.0f/255.0f,70.0f/255.0f,
		123.0f/255.0f,187.0f/255.0f,70.0f/255.0f,
		123.0f/255.0f,187.0f/255.0f,70.0f/255.0f
	};

	powerboard = create3DObject(GL_TRIANGLES, 4*3 ,GAME_SCOREBOARD, vertex_buffer_data, color_buffer_data, -400 + 30, -270 +15, 30, GL_FILL);
}

void createPig ()
{
	// GL3 accepts only Triangles. Quads are not supported
	double n=30;
	static GLfloat vertex_buffer_data[6][9*30+2*3 +9*15*2 + 9*15*2 + 9*15 + 9*15 * 2];
	static GLfloat color_buffer_data[6][9*30+2*3 + 9*15*2 + 9*15*2 + 9*15 + 9*15 * 2];
	double sizea[6]={18 + 5, 23 +5 , 20 + 5, 25 + 5, 20 + 5, 28 + 5},sizeb[6]={18, 23, 20, 25, 20, 28};
	double eyeline = 0.5;
	float angle=0;
	for(int j=0;j<6;j++)
		for(int i=0;i<n;i++){
			vertex_buffer_data[j][9*i] = vertex_buffer_data[j][9*i+1] = vertex_buffer_data[j][9*i+2] = 0;
			vertex_buffer_data[j][9*i+3]=sizea[j]*cos(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+4]=sizeb[j]*sin(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+5]=0;
			angle += 360.0f/n;
			vertex_buffer_data[j][9*i+6]=sizea[j]*cos(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+7]=sizeb[j]*sin(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+8]=0;

			color_buffer_data[j][9*i] = 114.0f/255.0f;
			color_buffer_data[j][9*i+1] = 194.0f/255.0f;
			color_buffer_data[j][9*i+2] = 65.0f/255.0f;
			color_buffer_data[j][9*i+3] = 114.0f/255.0f;
			color_buffer_data[j][9*i+4] = 194.0f/255.0f;
			color_buffer_data[j][9*i+5] = 65.0f/255.0f;
			color_buffer_data[j][9*i+6] = 114.0f/255.0f;
			color_buffer_data[j][9*i+7] = 194.0f/255.0f;
			color_buffer_data[j][9*i+8] = 65.0f/255.0f;
		}
	angle = 0;
	float ap;
	for(int j=0;j<6;j++,angle=0)
		for(int i=n;i<2*n;i++){
			if(i<n+(n/2))
				ap = sizea[j]/2;
			else
				ap = -sizea[j]/2;
			if(i==n+n/2) angle = 0;
			vertex_buffer_data[j][9*i]=ap,vertex_buffer_data[j][9*i+1]=-0.5,vertex_buffer_data[j][9*i+2] = 0;
			vertex_buffer_data[j][9*i+3]=ap+0.25*sizea[j]*cos(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+4]=-0.5+0.25*sizea[j]*sin(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+5]=0;
			angle += 360.0f/(n/2);
			vertex_buffer_data[j][9*i+6]=ap+0.25*sizea[j]*cos(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+7]=-0.5+0.25*sizea[j]*sin(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+8]=0;

			color_buffer_data[j][9*i] = 1;//.0f/255.0f;
			color_buffer_data[j][9*i+1] = 1;//194.0f/255.0f;
			color_buffer_data[j][9*i+2] = 1;//65.0f/255.0f;
			color_buffer_data[j][9*i+3] = 1;//114.0f/255.0f;
			color_buffer_data[j][9*i+4] = 1;//194.0f/255.0f;
			color_buffer_data[j][9*i+5] = 1;//65.0f/255.0f;
			color_buffer_data[j][9*i+6] = 1;//114.0f/255.0f;
			color_buffer_data[j][9*i+7] = 1;//194.0f/255.0f;
			color_buffer_data[j][9*i+8] = 1;//65.0f/255.0f;
		}
	for(int j=0;j<6;j++,angle=0)
		for(int i=2*n;i<3*n;i++){
			if(i<2*n+(n/2))
				ap = 0.41*sizea[j];
			else
				ap = -0.41*sizea[j];
			if(i==2*n+n/2) angle = 0;
			vertex_buffer_data[j][9*i]=ap,vertex_buffer_data[j][9*i+1]=-0.5,vertex_buffer_data[j][9*i+2] = 0;
			vertex_buffer_data[j][9*i+3]=ap+0.1*sizea[j]*cos(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+4]=-0.5+0.1*sizea[j]*sin(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+5]=0;
			angle += 360.0f/(n/2);
			vertex_buffer_data[j][9*i+6]=ap+0.1*sizea[j]*cos(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+7]=-0.5+0.1*sizea[j]*sin(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+8]=0;

			color_buffer_data[j][9*i] = 0;//.0f/255.0f;
			color_buffer_data[j][9*i+1] = 0;//194.0f/255.0f;
			color_buffer_data[j][9*i+2] = 0;//65.0f/255.0f;
			color_buffer_data[j][9*i+3] = 0;//114.0f/255.0f;
			color_buffer_data[j][9*i+4] = 0;//194.0f/255.0f;
			color_buffer_data[j][9*i+5] = 0;//65.0f/255.0f;
			color_buffer_data[j][9*i+6] = 0;//114.0f/255.0f;
			color_buffer_data[j][9*i+7] = 0;//194.0f/255.0f;
			color_buffer_data[j][9*i+8] = 0;//65.0f/255.0f;
		}
	angle = 0;
	for(int j=0;j<6;j++,angle=0)
		for(int i=3*n;i<3*n + n/2;i++){
			vertex_buffer_data[j][9*i]=0,vertex_buffer_data[j][9*i+1]=5,vertex_buffer_data[j][9*i+2] = 0;
			vertex_buffer_data[j][9*i+3]=0.25*sizea[j]*cos(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+4]=5+0.25*sizea[j]*sin(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+5]=0;
			angle += 360.0f/(n/2);
			vertex_buffer_data[j][9*i+6]=0.25*sizea[j]*cos(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+7]=5+0.25*sizea[j]*sin(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+8]=0;
//rgb(167,233,1)
			color_buffer_data[j][9*i] = 167.0f/255.0f;
			color_buffer_data[j][9*i+1] = 233.0f/255.0f;
			color_buffer_data[j][9*i+2] = 1.0f/255.0f;
			color_buffer_data[j][9*i+3] = 167.0f/255.0f;
			color_buffer_data[j][9*i+4] = 233.0f/255.0f;
			color_buffer_data[j][9*i+5] = 1.0f/255.0f;
			color_buffer_data[j][9*i+6] = 167.0f/255.0f;
			color_buffer_data[j][9*i+7] = 233.0f/255.0f;
			color_buffer_data[j][9*i+8] = 1.0f/255.0f;
		}
	angle = 0;
	for(int j=0;j<6;j++,angle=0)
		for(int i=3*n + n/2;i<4*n + n/2;i++){
			if(i<4*n)
				ap = 0.1*sizea[j];
			else
				ap = -0.1*sizea[j];
			if(i==4*n) angle = 0;
			vertex_buffer_data[j][9*i]=ap,vertex_buffer_data[j][9*i+1]=5,vertex_buffer_data[j][9*i+2] = 0;
			vertex_buffer_data[j][9*i+3]=ap+0.08*sizea[j]*cos(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+4]=5+0.08*sizea[j]*sin(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+5]=0;
			angle += 360.0f/(n/2);
			vertex_buffer_data[j][9*i+6]=ap+0.08*sizea[j]*cos(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+7]=5+0.08*sizea[j]*sin(angle*M_PI/180.0f);
			vertex_buffer_data[j][9*i+8]=0;
//rgb(31,55,24)
			color_buffer_data[j][9*i] = 31.0f/255.0f;
			color_buffer_data[j][9*i+1] = 55.0f/255.0f;
			color_buffer_data[j][9*i+2] = 24.0/255.0f;
			color_buffer_data[j][9*i+3] = 31.0f/255.0f;
			color_buffer_data[j][9*i+4] = 55.0f/255.0f;
			color_buffer_data[j][9*i+5] = 24.0f/255.0f;
			color_buffer_data[j][9*i+6] = 31.0f/255.0f;
			color_buffer_data[j][9*i+7] = 55.0f/255.0f;
			color_buffer_data[j][9*i+8] = 24.0f/255.0f;
		}
	piginitx[0]=50, piginitx[1]=345, piginitx[2] = 415, piginitx[3] = 280, piginitx[4] = 70,piginitx[5] = 100;
	// create3DObject creates and returns a handle to a VAO that can be used later
	pigs[0] = create3DObject(GL_TRIANGLES, 4*n*3 + (n/2) *3  ,GAME_PIG, vertex_buffer_data[0], color_buffer_data[0], 50, 200-sizeb[0], sizea[0], GL_FILL);
	pigs[1] = create3DObject(GL_TRIANGLES, 4*n*3 + (n/2) *3 ,GAME_PIG, vertex_buffer_data[1], color_buffer_data[1], 345, 200-50-sizeb[1], sizea[1], GL_FILL);
	pigs[2] = create3DObject(GL_TRIANGLES, 4*n*3 + (n/2) *3  ,GAME_PIG, vertex_buffer_data[2], color_buffer_data[2], 415, 200-sizeb[2], sizea[2], GL_FILL);
	pigs[3] = create3DObject(GL_TRIANGLES, 4*n*3 + (n/2) *3  ,GAME_PIG, vertex_buffer_data[3], color_buffer_data[3], 280, 200-50-40-sizeb[3], sizea[3], GL_FILL);
	pigs[4] = create3DObject(GL_TRIANGLES, 4*n*3 + (n/2) *3  ,GAME_PIG, vertex_buffer_data[4], color_buffer_data[4], 70, -110 - 10- sizeb[4], sizea[4], GL_FILL);
	pigs[5] = create3DObject(GL_TRIANGLES, 4*n*3 + (n/2) *3  ,GAME_PIG, vertex_buffer_data[5], color_buffer_data[5], 100, -210 - 10- sizeb[5], sizea[4], GL_FILL);

	pig_wood[3] = 1;
	pig_wood[1] = 2;
}
// Creates the rectangle object used in this sample code
void createCannonball ()
{
	// GL3 accepts only Triangles. Quads are not supported
	double n=20;
	static GLfloat vertex_buffer_data[9*20 + 2*3 + 2*9*20 + 9*20];
	static GLfloat color_buffer_data [9*20 + 2*3 + 2*9*20 +9*20];
	float angle=0;
	for(int i=0;i<n;i++){
		vertex_buffer_data[9*i] = vertex_buffer_data[9*i+1] = vertex_buffer_data[9*i+2] = 0;
		vertex_buffer_data[9*i+3]=cannonball_size*cos(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+4]=cannonball_size*sin(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+5]=0;
		angle += 360.0f/n;
		vertex_buffer_data[9*i+6]=cannonball_size*cos(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+7]=cannonball_size*sin(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+8]=0;

		color_buffer_data[9*i] = 214.0f/255.0f;
		color_buffer_data[9*i+1] = 1.0f/255.0f;
		color_buffer_data[9*i+2] = 14.0f/255.0f;
		color_buffer_data[9*i+3] = 214.0f/255.0f;
		color_buffer_data[9*i+4] = 1.0f/255.0f;
		color_buffer_data[9*i+5] = 14.0f/255.0f;
		color_buffer_data[9*i+6] = 214.0f/255.0f;
		color_buffer_data[9*i+7] = 1.0f/255.0f;
		color_buffer_data[9*i+8] = 14.0f/255.0f;
	}
	angle= 0;
	for(int i=n;i<n+n;i++){
		vertex_buffer_data[9*i] =5; vertex_buffer_data[9*i+1] =-2; vertex_buffer_data[9*i+2] = 0;
		vertex_buffer_data[9*i+3]=5+cannonball_size*0.25*cos(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+4]=-2+cannonball_size*0.5*sin(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+5]=0;
		angle += 360.0f/(n);
		vertex_buffer_data[9*i+6]=5+cannonball_size*0.25*cos(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+7]=-2+cannonball_size*0.5*sin(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+8]=0;

		color_buffer_data[9*i] = 1;
		color_buffer_data[9*i+1] = 1;
		color_buffer_data[9*i+2] = 1;
		color_buffer_data[9*i+3] = 1;
		color_buffer_data[9*i+4] = 1;
		color_buffer_data[9*i+5] = 1;
		color_buffer_data[9*i+6] = 1;
		color_buffer_data[9*i+7] = 1;
		color_buffer_data[9*i+8] = 1;
	}
	angle =0 ;
	for(int i=2*n;i<2*n+n;i++){
		vertex_buffer_data[9*i] = 5;vertex_buffer_data[9*i+1] =0; vertex_buffer_data[9*i+2] = 0;
		vertex_buffer_data[9*i+3]=5+0.15*cannonball_size*cos(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+4]=0.15*cannonball_size*sin(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+5]=0;
		angle += 360.0f/n;
		vertex_buffer_data[9*i+6]=5+0.15*cannonball_size*cos(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+7]=0.15*cannonball_size*sin(angle*M_PI/180.0f);
		vertex_buffer_data[9*i+8]=0;

		color_buffer_data[9*i] = 0;
		color_buffer_data[9*i+1] = 0;
		color_buffer_data[9*i+2] = 0;
		color_buffer_data[9*i+3] = 0;
		color_buffer_data[9*i+4] = 0;
		color_buffer_data[9*i+5] = 0;
		color_buffer_data[9*i+6] = 0;
		color_buffer_data[9*i+7] = 0;
		color_buffer_data[9*i+8] = 0;
	}

	for(int i=9*3*n;i<9*(3*n+2);i+=3){
		color_buffer_data[i] =252.0f/255.0f;
		color_buffer_data[i+1] = 187.0f/255.0f;
		color_buffer_data[i+2] = 35.0f/255.0f;
	}

	vertex_buffer_data[9*20*3] = cannonball_size*cos((360.0f/n) *M_PI/180.0f);
	vertex_buffer_data[9*20*3+1] = cannonball_size*sin((360.0f/n) *M_PI/180.0f);
	vertex_buffer_data[9*20*3+2] = 0;
	vertex_buffer_data[9*20*3+3] = cannonball_size+10;
	vertex_buffer_data[9*20*3+4] = -2;
	vertex_buffer_data[9*20*3+5] = 0;
	vertex_buffer_data[9*20*3+6] = cannonball_size*cos((360.0f/n) *M_PI/180.0f);
	vertex_buffer_data[9*20*3+7] = -cannonball_size*sin((360.0f/n) *M_PI/180.0f);
	vertex_buffer_data[9*20*3+8] = 0;
	// create3DObject creates and returns a handle to a VAO that can be used later
	cannonball = create3DObject(GL_TRIANGLES, 3*n*3 + 2*3, GAME_BIRD, vertex_buffer_data, color_buffer_data,  0, 0, cannonball_size, GL_FILL);
}

void createGameFloor ()
{
	static GLfloat vertex_buffer_data [20*18];

	double base=200;
	for(int i=0;i<20;i++){
		vertex_buffer_data[18*i]=-600,vertex_buffer_data[18*i+1]=base,vertex_buffer_data[18*i+2]=0;
		vertex_buffer_data[18*i+3]=-600,vertex_buffer_data[18*i+4]=base+10,vertex_buffer_data[18*i+5]=0;
		vertex_buffer_data[18*i+6]=600,vertex_buffer_data[18*i+7]=base,vertex_buffer_data[18*i+8]=0;
		vertex_buffer_data[18*i+9]=600,vertex_buffer_data[18*i+10]=base+10,vertex_buffer_data[18*i+11]=0;
		vertex_buffer_data[18*i+12]=-600,vertex_buffer_data[18*i+13]=base+10,vertex_buffer_data[18*i+14]=0;
		vertex_buffer_data[18*i+15]=600,vertex_buffer_data[18*i+16]=base,vertex_buffer_data[18*i+17]=0;
		base+=5;
	}

	static  GLfloat color_buffer_data [20*18]; 
	double gred=133.0/255.0f,ggreen=183.0/255.0f,gblue=52.0/255.0f;
	for(int i=0;i<20;i++){
		color_buffer_data[18*i]=gred,color_buffer_data[18*i+1]=ggreen,color_buffer_data[18*i+2]=gblue;
		color_buffer_data[18*i+3]=gred,color_buffer_data[18*i+4]=ggreen,color_buffer_data[18*i+5]=gblue;
		color_buffer_data[18*i+6]=gred,color_buffer_data[18*i+7]=ggreen,color_buffer_data[18*i+8]=gblue;
		color_buffer_data[18*i+9]=gred,color_buffer_data[18*i+10]=ggreen,color_buffer_data[18*i+11]=gblue;
		color_buffer_data[18*i+12]=gred,color_buffer_data[18*i+13]=ggreen,color_buffer_data[18*i+14]=gblue;
		color_buffer_data[18*i+15]=gred,color_buffer_data[18*i+16]=ggreen,color_buffer_data[18*i+17]=gblue;
		ggreen+=10.0f/255.0f;
		gred+=2.5f/255.0f;
	}
	gameFloor = create3DObject(GL_TRIANGLES, 20*6, GAME_WOOD_HORIZONTAL, vertex_buffer_data, color_buffer_data, GL_FILL, fireposx, fireposy, 25);
}

GLfloat woodsizex[6],woodsizey[6];
void createWoodLogs(){


	static GLfloat vertex_buffer_data[6][18];
	woodsizex[0] = 10;
	woodsizey[0] = 30;
	woodsizex[1] = 35;
	woodsizey[1] = 20;
	woodsizex[2] = 75;
	woodsizey[2] = 25;
	woodsizex[3] = 10;
	woodsizey[3] = 100;
	woodsizex[4] = 50;
	woodsizey[4] = 10;
	woodsizex[5] = 50;
	woodsizey[5] = 10;

	for(int i=0;i<=5;i++){
		vertex_buffer_data[i][0] = vertex_buffer_data[i][3] = vertex_buffer_data[i][12] = -woodsizex[i];
		vertex_buffer_data[i][6] = vertex_buffer_data[i][9] = vertex_buffer_data[i][15] = woodsizex[i];
		vertex_buffer_data[i][1] = vertex_buffer_data[i][7] = vertex_buffer_data[i][10] = woodsizey[i];
		vertex_buffer_data[i][4] = vertex_buffer_data[i][13] = vertex_buffer_data[i][16] = -woodsizey[i];
	}
	static const GLfloat color_buffer_data [] = {
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f,
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f,
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f,
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f,
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f,
		228.0f/255.0f,142.0f/255.0f,57.0f/255.0f
	};
	static const GLfloat color_buffer_data3 [] = {
		212.0f/255.0f,121.0f/255.0f,52.0f/255.0f,
		212.0f/255.0f,121.0f/255.0f,52.0f/255.0f,
		212.0f/255.0f,121.0f/255.0f,52.0f/255.0f,
		212.0f/255.0f,121.0f/255.0f,52.0f/255.0f,
		212.0f/255.0f,121.0f/255.0f,52.0f/255.0f,
		212.0f/255.0f,121.0f/255.0f,52.0f/255.0f
	};
	woodlogs[0] = create3DObject(GL_TRIANGLES, 6, GAME_WOOD_VERTICAL, vertex_buffer_data[0], color_buffer_data, GL_FILL, 0, 0, 25);
	woodlogs[1] = create3DObject(GL_TRIANGLES, 6, GAME_WOOD_HORIZONTAL, vertex_buffer_data[1], color_buffer_data, 280, 130, 25, GL_FILL);
	woodlogs[2] = create3DObject(GL_TRIANGLES, 6, GAME_WOOD_HORIZONTAL, vertex_buffer_data[2], color_buffer_data3, 310, 175, 25, GL_FILL);
	woodlogs[3] = create3DObject(GL_TRIANGLES, 6, GAME_WOOD_VERTICAL, vertex_buffer_data[3], color_buffer_data3, 150, -200, 25, GL_FILL);
	woodlogs[4] = create3DObject(GL_TRIANGLES, 6, GAME_WOOD_VERTICAL, vertex_buffer_data[4], color_buffer_data3, 90, -110, 25, GL_FILL);
	woodlogs[5] = create3DObject(GL_TRIANGLES, 6, GAME_WOOD_VERTICAL, vertex_buffer_data[4], color_buffer_data3, 90, -210, 25, GL_FILL);

}

void createBackground(GLuint textureID){
	static const GLfloat vertex_buffer_data[] = {
		-600, -300, 0,
		-600, 300, 0,
		600, -300, 0,

		-600, 300, 0,
		600, -300, 0,
		600, 300, 0
	};

	static const GLfloat texture_buffer_data[] = {
		0,0,
		0,1,
		1,0,

		0,1,
		1,0,
		1,1
	};

	background = create3DTexturedObject(GL_TRIANGLES, 6, vertex_buffer_data, texture_buffer_data, textureID, GL_FILL);
}

void createCatapult(){
	static const GLfloat vertex_buffer_data[] = {
		-0.5, -4, 0,
		-0.5, 4, 0,
		0.5, -4, 0,

		-0.5, 4, 0,
		0.5, -4, 0,
		0.5, 4, 0
	};

	static const GLfloat color_buffer_data[] = {
		86.0f/255.0f,38.0f/255.0f,15.0f/255.0f,
		86.0f/255.0f,38.0f/255.0f,15.0f/255.0f,
		86.0f/255.0f,38.0f/255.0f,15.0f/255.0f,
		86.0f/255.0f,38.0f/255.0f,15.0f/255.0f,
		86.0f/255.0f,38.0f/255.0f,15.0f/255.0f,
		86.0f/255.0f,38.0f/255.0f,15.0f/255.0f
	};
	catapult = create3DObject(GL_TRIANGLES, 2*3 ,GAME_WOOD_HORIZONTAL, vertex_buffer_data, color_buffer_data, 50, 200, 0, GL_FILL);
}
float camera_rotation_angle = 90;
float rectangle_rotation = 0;
int lineorfill=GL_FILL;
/* Render the scene with openGL */
/* Edit this function according to your assignment */

VAO *temp ;
void createtemp(){
	static const GLfloat vertex_buffer_data [] = {
		0, 0, 0,
		0, -100, 0,
		100, 0, 0
	};
	 static const GLfloat color_buffer_data [] ={
		 1,0,0,1,0,0,1,0,0};
	temp = create3DObject(GL_TRIANGLES, 3 ,GAME_WOOD_HORIZONTAL, vertex_buffer_data, color_buffer_data, 50, 200, 0, GL_FILL);
}

void draw ()
{
	// clear the color and depth in the frame buffer
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	// use the loaded shader program
	// Don't change unless you know what you are doing
	glUseProgram (programID);

	// Eye - Location of camera. Don't change unless you are sure!!
	glm::vec3 eye ( 5*cos(camera_rotation_angle*M_PI/180.0f), 0, 5*sin(camera_rotation_angle*M_PI/180.0f) );
	// Target - Where is the camera looking at.  Don't change unless you are sure!!
	glm::vec3 target (0, 0, 0);
	// Up - Up vector defines tilt of camera.  Don't change unless you are sure!!
	glm::vec3 up (0, 1, 0);

	// Compute Camera matrix (view)
	//Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
	//  Don't change unless you are sure!!
	Matrices.view = glm::lookAt(glm::vec3(0,0,3), glm::vec3(0,0,0), glm::vec3(0,1,0)); // Fixed camera for 2D (ortho) in XY plane

	// Compute ViewProject matrix as view/camera might not be changed for this frame (basic scenario)
	//  Don't change unless you are sure!!
	glm::mat4 VP = Matrices.projection * Matrices.view;

	// Send our transformation to the currently bound shader, in the "MVP" uniform
	// For each model you render, since the MVP will be different (at least the M part)
	//  Don't change unless you are sure!!
	glm::mat4 MVP;	// MVP = Projection * View * Model

	
	//Displaying background using texture
	glUseProgram(textureProgramID);

	Matrices.model = glm::mat4(1.0f);
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.TexMatrixID, 1, GL_FALSE, &MVP[0][0]);
	glUniform1i(glGetUniformLocation(textureProgramID, "texSampler"), 0);
	draw3DTexturedObject(background);
	
	
	glUseProgram (programID);
	// Load identity to model matrix
	Matrices.model = glm::mat4(1.0f);

	/* Render your scene */

	//glm::mat4 translateTriangle = glm::translate (glm::vec3(-2.0f, 0.0f, 0.0f)); // glTranslatef
	//glm::mat4 rotateTriangle = glm::rotate((float)(triangle_rotation*M_PI/180.0f), glm::vec3(0,0,1));  // rotate about vector (1,0,0)
	//glm::mat4 triangleTransform = translateTriangle * rotateTriangle;
	//  Matrices.model *= triangleTransform; 
	MVP = VP * Matrices.model; // MVP = p * V * M

	//  Don't change unless you are sure!!
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	// draw3DObject draws the VAO given to it using current MVP matrix
	//  draw3DObject(triangle);

	// Pop matrix to undo transformations till last push matrix instead of recomputing model matrix
	// glPopMatrix ();
	//Displaying pigs and counting them for score 
	int cnt = 0;
	for(int i=0;i<6;i++){
		if(!pigs[i]->dead){
			double x1 = cannonball->centerx,y1 = cannonball->centery, x2 = pigs[i]->centerx, y2 = pigs[i]->centery;
			if(pigs[i]->radius + cannonball->radius > sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1))){
				pigs[i]->dead = 1;
				scoretimer[i][0] = pigs[i]->centerx; 
				scoretimer[i][1] = pigs[i]->centery; 
				scoretimer[i][2] = tim;
				speedx = 0.1*speedx;
				speedy = 0.1*speedy;
			}

			Matrices.model = glm::mat4(1.0f);
			glm::mat4 translatePig = glm::translate(glm::vec3(pigs[i]->centerx + pigspx[i],pigs[i]->centery + pigspy[i],0));
			pigs[i]->centerx += pigspx[i];
			pigs[i]->centery += pigspy[i];
			glm::mat4 rotatePig = glm::rotate((float)((pigs[i]->centerx-piginitx[i])/pigs[i]->radius),glm::vec3(0,0,1));
			pigspx[i]/= 1.02;
			Matrices.model *= (translatePig*rotatePig);
			MVP = VP  * Matrices.model; 
			glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
			draw3DObject(pigs[i]);

		}
		else
			cnt++;
	}

	//Displaying game floor
	Matrices.model = glm::mat4(1.0f);
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	draw3DObject(gameFloor);
	
	//Displaying power board
	Matrices.model = glm::mat4(1.0f);
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	draw3DObject(powerboard);

	//Checking collisions between pigs and wood logs
	Matrices.model = glm::mat4(1.0f);
	glm::mat4 translateWoodlog,rotateWoodlog;
	translateWoodlog = glm::translate(glm::vec3(0,170,0));
	if(collision_state==1){
		if(pivotx == -10){
			translateWoodlog = glm::translate(glm::vec3(10,200,0));
			glm::mat4 translateWoodlog2 = glm::translate(glm::vec3(pivotx,pivoty,0));
			rotateWoodlog = glm::rotate((float)(angle[0]*M_PI/180.0f), glm::vec3(0,0,1));
			angle[0] += angular_v[0];
			angular_v[0] += 0.3;
			angle[0] = min(angle[0],90.0);
			Matrices.model *= (translateWoodlog*rotateWoodlog*translateWoodlog2);
			if(angle[0] >= 45){
				pigs[0]->dead = 1;
				scoretimer[0][0]=pigs[0]->centerx;
				scoretimer[0][1]=pigs[0]->centery;
				scoretimer[0][2]=tim;
			}
		}
		else{
			translateWoodlog = glm::translate(glm::vec3(-10,200,0));
			glm::mat4 translateWoodlog2 = glm::translate(glm::vec3(pivotx,pivoty,0));
			rotateWoodlog = glm::rotate((float)(angle[0]*M_PI/180.0f), glm::vec3(0,0,1));
			angle[0] -= angular_v[0];
			angular_v[0] += 0.3;
			angle[0] = max(angle[0],-90.0);
			Matrices.model *= (translateWoodlog*rotateWoodlog*translateWoodlog2);
			if(angle[0] >= 45){
				pigs[0]->dead = 1;
				scoretimer[0][0]=pigs[0]->centerx;
				scoretimer[0][1]=pigs[0]->centery;
				scoretimer[0][2]=tim;
			}
		}
	}
	else
		Matrices.model *= translateWoodlog;
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	draw3DObject(woodlogs[0]);

	//Displaying wood logs
	for(int i=1;i<=5;i++){
		Matrices.model = glm::mat4(1.0f);
		translateWoodlog = glm::translate(glm::vec3(woodlogs[i]->centerx + woodspx[i],woodlogs[i]->centery,0));
		woodlogs[i]->centerx += woodspx[i];
		woodspx[i] /= 1.02;
		Matrices.model *= translateWoodlog;
		MVP = VP * Matrices.model;
		glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
		draw3DObject(woodlogs[i]);
		if(i<=2&&woodlogs[i]->centerx + woodsizex[i] > pigs[i]->centerx - pigs[i]->radius){
			pigspx[i] = woodspx[i]*0.95;
			woodspx[i]=woodspx[i]*0.9;
			pigs[i]->centerx = woodlogs[i]->centerx + woodsizex[i] + pigs[i]->radius;
		}
	}

	//Checking collisions between pigs and pigs
	for(int i=0;i<4;i++) {
		if(pig_wood[i]!=0){
			int j = pig_wood[i];
			
			if(j==1){
				if(pigs[i]->centerx+pigs[i]->radius<woodlogs[j]->centerx - woodsizex[j])
					pigspy[i]+=gravity/3.0f;
				if((pigs[i]->centery+pigs[i]->radius > woodlogs[j+1]->centery - woodsizey[j+1])
						||pigs[i]->centery + pigs[i]->radius > 200){
					pigs[i]->dead=1;
					scoretimer[i][0] = pigs[i]->centerx; 
					scoretimer[i][1] = pigs[i]->centery; 
					scoretimer[i][2] = tim;
				}

			}
			if(j==2){
				if(pigs[i]->centerx-pigs[i]->radius>woodlogs[j]->centerx + woodsizex[j])
					pigspy[i]+=gravity/3.0f;
				double x1 = pigs[i]->centerx, y1 = pigs[i]->centery, x2 = pigs[i+1]->centerx, y2 = pigs[i+1]->centery;
				if(sqrt((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2)) < pigs[i]->radius + pigs[i+1]->radius)
				{
					pigs[i]->dead=pigs[i+1]->dead = 1;
					scoretimer[i][0] = pigs[i]->centerx; 
					scoretimer[i][1] = pigs[i]->centery; 
					scoretimer[i][2]=tim;
					scoretimer[i+1][0] = pigs[i+1]->centerx; 
					scoretimer[i+1][1] = pigs[i+1]->centery; 
					scoretimer[i+1][2]=tim;
				}
				if(pigs[i]->centery+pigs[i]->radius>200){
					pigs[i]->dead=1;
					scoretimer[i][0] = pigs[i]->centerx; 
					scoretimer[i][1] = pigs[i]->centery; 
					scoretimer[i][2]=tim;
				}
				
			}
		}
	}

	//Controlling bird using keyboard
	if(keyboard_pressed_statex == 1){
		keyboardy -= 2;
		if(keyboardx < fireposx)
			keyboardx += 2;
		else if(keyboardx > fireposx)
			keyboardx -= 2;
		curx = keyboardx;
		cury = keyboardy;
	}
	if(keyboard_pressed_statey == 1){
		keyboardy += 2;
		if(keyboardx < fireposx)
			keyboardx -=2;
		else if(keyboardx >fireposx)
			keyboardx += 2;
		curx = keyboardx;
		cury = keyboardy;
	}
		
	//Limiting the power with which bird can be shot
	if(pressed_state==1 || keyboard_pressed_statex == 1 || keyboard_pressed_statey == 1)
		if(sqrt((curx-initx)*(curx-initx)+(cury-inity)*(cury-inity)) > 70){
			double angle_present = -M_PI+atan2(inity-cury,initx-curx);
			curx = initx + 70*cos(angle_present);
			cury = inity + 70*sin(angle_present);
		}
	
	//Displaying catapult
	Matrices.model = glm::mat4(1.0f);
	glm::mat4 translateCatapult = glm::translate(glm::vec3(-0.5, -4, 0));
	glm::mat4 translateCatapult2 = glm::translate(glm::vec3(fireposx-10 , fireposy+10 , 0));
	glm::mat4 rotateCatapult = glm::rotate((float)(atan2(-cury+fireposy+10 ,-curx+fireposx-10)), glm::vec3(0,0,1));
	double scalelength2 = sqrt((fireposx-10 - curx)*(fireposx -10 -curx) + (fireposy+10 - cury)*(fireposy+10-cury));
	glm::mat4 scaleCatapult = glm::scale(glm::vec3(scalelength2 , 1, 1));
	if(pressed_state == 1 || keyboard_pressed_statex == 1 || keyboard_pressed_statey == 1)
		Matrices.model *= (translateCatapult2 * rotateCatapult *  scaleCatapult * translateCatapult);
	else
		Matrices.model *= translateCatapult;
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	if(pressed_state==1)
		draw3DObject(catapult);

	//Displaying the bird
	Matrices.model = glm::mat4(1.0f);
	glm::mat4 translateRectangle;// = glm::translate (glm::vec3(curx, cury, 0));        // glTranslatef
	glm::mat4 rotateRectangle; // rotate about vector (-1,1,1)
	if(pressed_state==0 && keyboard_pressed_statex == 0 && keyboard_pressed_statey == 0){
		translateRectangle = glm::translate (glm::vec3(fireposx, fireposy, 0));        // glTranslatef
		cannonball->centerx = fireposx;
		cannonball->centery = fireposy;
		cannonball->radius = cannonball_size;
	}
	else if(pressed_state==1 || keyboard_pressed_statex == 1 || keyboard_pressed_statey == 1){
		if(sqrt((curx-initx)*(curx-initx)+(cury-inity)*(cury-inity)) > 70){
			double angle_present = -M_PI+atan2(inity-cury,initx-curx);
			curx = initx + 70*cos(angle_present);
			cury = inity + 70*sin(angle_present);
		}
		power = sqrt((curx-initx)*(curx-initx) + (cury-inity)*(cury-inity));
		translateRectangle = glm::translate (glm::vec3(curx, cury, 0));        // glTranslatef
		cannonball->centerx = curx;
		cannonball->centery = cury;
		cannonball->radius = cannonball_size;
	}
	else {
		translateRectangle = glm::translate (glm::vec3(initx, inity, 0));        // glTranslatef
		cannonball->centerx = initx;
		cannonball->centery = inity;
		cannonball->radius = cannonball_size;
	}
	if(collision_state == 0 && cannonball->centerx >= -10 - cannonball_size && cannonball->centerx <= -10 + 20 + cannonball_size && cannonball->centery >= 140 - cannonball_size ){
		collision_state=1;
		angle[0] = 0.1;
		angular_v[0] = speedx*0.2;
		if(cannonball->centerx > 10){
			pivotx = 10;
			angle[0] = -0.1;
			angular_v[0] = -speedx*0.2;
		}
		if(cannonball->centery < 140 && cannonball->centerx > -10 - cannonball_size/2 && (cannonball->centerx < 10 + cannonball_size/2)){
			speedy = -0.5*speedy;
			inity = 140 - cannonball_size - 1;
		}
		else{
			speedx = -0.5*speedx;
		}
	}

	for(int i=1;i<=5;i++){
		double x = cannonball->centerx,y = cannonball->centery;
		if(x >= woodlogs[i]->centerx - woodsizex[i] - cannonball_size && x <= woodlogs[i]->centerx + woodsizex[i] + cannonball_size && y >= woodlogs[i]->centery - woodsizey[i] - cannonball_size && y <= woodlogs[i]->centery + woodsizey[i]) {
			if(cannonball->centery < woodlogs[i]->centery - woodsizey[i] && cannonball->centerx > woodlogs[i]->centerx - woodsizex[i] - cannonball_size/2 ){
				speedy = -0.25*speedy, speedx = 0.25*speedx;
				inity = woodlogs[i]->centery - woodsizey[i] - cannonball_size - 1;
			}
			else {
				if(i==1){
					woodspx[1] = speedx * 0.1;
					woodspx[2] = woodspx[1] * 0.1;
				}
				else if(i==2){
					woodspx[2] = speedx * 0.05;
					woodspx[1] = woodspx[2] * 0.5;
				}
				speedx = -0.25*speedx, speedy = 0.25*speedy;
				initx = woodlogs[i]->centerx - woodsizex[i] - cannonball_size - 1;
			}
			break;
		}
	}
	if(pressed_state==3) 
		rotateRectangle = glm::rotate((float)(atan2(-prevy+inity,-prevx+initx)), glm::vec3(0,0,1)); // rotate about vector (-1,1,1)
	else
		rotateRectangle = glm::rotate((float)(atan2(-cury+inity,-curx+initx)), glm::vec3(0,0,1)); // rotate about vector (-1,1,1)

	if(pressed_state==3 || pressed_state==1 || keyboard_pressed_statex == 1 || keyboard_pressed_statey)  Matrices.model *= (translateRectangle * rotateRectangle);
	else  Matrices.model *= (translateRectangle );
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);

	// draw3DObject draws the VAO given to it using current MVP matrix
	draw3DObject(cannonball);


	//Displaying power
	Matrices.model = glm::mat4(1.0f);
	translateCatapult = glm::translate(glm::vec3(-0.5, -4, 0));
	translateCatapult2 = glm::translate(glm::vec3(fireposx + 20, fireposy + 15, 0));
	rotateCatapult = glm::rotate((float)(atan2(-cury+fireposy + 15,-curx+fireposx + 20)), glm::vec3(0,0,1));
	double scalelength = sqrt((fireposx+20 - curx)*(fireposx + 20 -curx) + (fireposy+15 - cury)*(fireposy+15-cury));
	scaleCatapult = glm::scale(glm::vec3(scalelength , 1, 1));
	if(pressed_state == 1 || keyboard_pressed_statex == 1 || keyboard_pressed_statey == 1)
		Matrices.model *= (translateCatapult2 * rotateCatapult *  scaleCatapult * translateCatapult);
	else
		Matrices.model *= translateCatapult;
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	if(pressed_state ==1) draw3DObject(catapult);

	Matrices.model = glm::mat4(1.0f);
	glm::mat4 scalePower = glm::scale(glm::vec3(power*6,1,1));
	glm::mat4 translatePower = glm::translate(glm::vec3(-400 - ( 90 - power * 3), -240, 0));
	Matrices.model *= ( translatePower * scalePower);
	MVP = VP * Matrices.model;
	glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
	draw3DObject(powerelement);


	// Increment angles
	float increments = 1;

	if(pressed_state==3){
		prevx=initx,prevy=inity;
		initx=initx+speedx,inity=inity+speedy,speedy+=gravity;
		inity=min(200-cannonball_size,inity);
		if(inity==200-cannonball_size)
			speedy=-0.8*speedy,speedx=0.7*speedx;
		if(fabs(speedx)<=0.05&&fabs(speedy)<=0.05){
				pressed_state=0;
				gravity = 0.2;
				power = 0;
			
		}
	}

	//Rendering score
	
	// Render font on screen
	static int fontScale = 1;
	float fontScaleValue = 50 + 0.25*sinf(fontScale*M_PI/180.0f);
	glm::vec3 fontColor = glm::vec3(228.0f/255.0f,142.0f/255.0f,57.0f/255.0f);//getRGBfromHue (fontScale);
				



	// Use font Shaders for next part of code
	glUseProgram(fontProgramID);
	Matrices.view = glm::lookAt(glm::vec3(0,0,3), glm::vec3(0,0,0), glm::vec3(0,1,0)); // Fixed camera for 2D (ortho) in XY plane

	// Transform the text
	Matrices.model = glm::mat4(1.0f);
	glm::mat4 translateText = glm::translate(glm::vec3(400,-250,0));
	glm::mat4 scaleText = glm::scale(glm::vec3(fontScaleValue,fontScaleValue,fontScaleValue));
	glm::mat4 rotateText = glm::rotate((float)M_PI, glm::vec3(1,0,0));
	Matrices.model *= (translateText * scaleText * rotateText);
	MVP = Matrices.projection * Matrices.view * Matrices.model;
	// send font's MVP and font color to fond shaders
	glUniformMatrix4fv(GL3Font.fontMatrixID, 1, GL_FALSE, &MVP[0][0]);
	glUniform3fv(GL3Font.fontColorID, 1, &fontColor[0]);
	
	score = cnt * 100;
	//string str = to_string(score);
	char str[10];
	sprintf(str,"SCORE: %d",score);
	// Render font
	GL3Font.font->Render(str);
	for(int i=0;i<6;i++){
		if(scoretimer[i][3]>0){
			Matrices.model = glm::mat4(1.0f);
			//cout<<scoretimer[i][0]<<" "<<scoretimer[i][1]<<endl;
			glm::mat4 translateText = glm::translate(glm::vec3(400,0,0));
			Matrices.model *=  ( translateText *scaleText * rotateText);
			MVP = Matrices.projection * Matrices.view * Matrices.model;
			glUniformMatrix4fv(GL3Font.fontMatrixID, 1, GL_FALSE, &MVP[0][0]);
			glUniform3fv(GL3Font.fontColorID, 1, &fontColor[0]);
	//		GL3Font.font->Render("100");
		}
	}
}

/* Initialise glfw window, I/O callbacks and the renderer to use */
/* Nothing to Edit here */
GLFWwindow* initGLFW (int width, int height)
{
	GLFWwindow* window; // window desciptor/handle

	glfwSetErrorCallback(error_callback);
	if (!glfwInit()) {
		exit(EXIT_FAILURE);
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(width, height, "Angry birds", NULL, NULL);

	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
	glfwSwapInterval( 1 );

	/* --- register callbacks with GLFW --- */

	/* Register function to handle window resizes */
	/* With Retina display on Mac OS X GLFW's FramebufferSize
	   is different from WindowSize */
	glfwSetFramebufferSizeCallback(window, reshapeWindow);
	glfwSetWindowSizeCallback(window, reshapeWindow);

	/* Register function to handle window close */
	glfwSetWindowCloseCallback(window, quit);

	/* Register function to handle keyboard input */
	glfwSetKeyCallback(window, keyboard);      // general keyboard input
	glfwSetCharCallback(window, keyboardChar);  // simpler specific character handling

	/* Register function to handle mouse click */
	glfwSetMouseButtonCallback(window, mouseButton);  // mouse button clicks
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetScrollCallback(window, scroll_callback);
	return window;
}

/* Initialize the OpenGL rendering properties */
/* Add all the models to be created here */
void initGL (GLFWwindow* window, int width, int height)
{
	// Load Textures
	// Enable Texture0 as current texture memory
	glActiveTexture(GL_TEXTURE0);
	
	// load an image file directly as a new OpenGL texture
	// GLuint texID = SOIL_load_OGL_texture ("beach.png", SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_TEXTURE_REPEATS); // Buggy for OpenGL3
	GLuint textureID = createTexture("background.png");
	
	// check for an error during the load process
	if(textureID == 0 )
		cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;

	// Create and compile our GLSL program from the texture shaders
	textureProgramID = LoadShaders( "TextureRender.vert", "TextureRender.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.TexMatrixID = glGetUniformLocation(textureProgramID, "MVP");


	/* Objects should be created before any other gl function and shaders */
	// Create the models
	// Generate the VAO, VBOs, vertices data & copy into the array buffer
	createBackground (textureID);
	createCannonball ();
	createGameFloor ();
	createWoodLogs();
	createPig();
	createPowerBoard();
	createPowerElement();
	createCatapult();
	createtemp();
	//createCatapult2();

	// Create and compile our GLSL program from the shaders
	programID = LoadShaders( "Sample_GL.vert", "Sample_GL.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.MatrixID = glGetUniformLocation(programID, "MVP");


	reshapeWindow (window, width, height);

	// Background color of the scene
	glClearColor(156.0/255.0f,205.0f/255.0f,237.0f/255.0f,0.0f);// (0.3f, 0.3f, 0.3f, 0.0f); // R, G, B, A
	glClearDepth (1.0f);

	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LEQUAL);
	
	// Initialise FTGL stuff
	//const char* fontfile = "UpsideDown.ttf";
	const char* fontfile = "arial.ttf";
	GL3Font.font = new FTExtrudeFont(fontfile); // 3D extrude style rendering

	if(GL3Font.font->Error())
	{
		cout << "Error: Could not load font `" << fontfile << "'" << endl;
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	// Create and compile our GLSL program from the font shaders
	fontProgramID = LoadShaders( "fontrender.vert", "fontrender.frag" );
	GLint fontVertexCoordAttrib, fontVertexNormalAttrib, fontVertexOffsetUniform;
	fontVertexCoordAttrib = glGetAttribLocation(fontProgramID, "vertexPosition");
	fontVertexNormalAttrib = glGetAttribLocation(fontProgramID, "vertexNormal");
	fontVertexOffsetUniform = glGetUniformLocation(fontProgramID, "pen");
	GL3Font.fontMatrixID = glGetUniformLocation(fontProgramID, "MVP");
	GL3Font.fontColorID = glGetUniformLocation(fontProgramID, "fontColor");

	//GL3Font.font->ShaderLocations(fontVertexCoordAttrib, fontVertexNormalAttrib, fontVertexOffsetUniform);
	GL3Font.font->FaceSize(1);
	GL3Font.font->Depth(0);
	GL3Font.font->Outset(0, 0);
	GL3Font.font->CharMap(ft_encoding_unicode);

	cout << "VENDOR: " << glGetString(GL_VENDOR) << endl;
	cout << "RENDERER: " << glGetString(GL_RENDERER) << endl;
	cout << "VERSION: " << glGetString(GL_VERSION) << endl;
	cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
}

int main (int argc, char** argv)
{
	int width = 1200;
	int height = 600;

	GLFWwindow* window = initGLFW(width, height);

	initGL (window, width, height);

	double last_update_time = glfwGetTime(), current_time;
	
	
	pid = fork();
	if(pid==0){
		while(1){
		mpg123_handle *mh;
		unsigned char *buffer;
		size_t buffer_size;
		size_t done;
		int err;

		int driver;
		ao_device *dev;

		ao_sample_format format;
		int channels, encoding;
		long rate;


		/* initializations */
		ao_initialize();
		driver = ao_default_driver_id();
		mpg123_init();
		mh = mpg123_new(NULL, &err);
		buffer_size = mpg123_outblock(mh);
		buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));

		/* open the file and get the decoding format */
		mpg123_open(mh, "trial.mp3");
		mpg123_getformat(mh, &rate, &channels, &encoding);

		/* set the output format and open the output device */
		format.bits = mpg123_encsize(encoding) * BITS;
		format.rate = rate;
		format.channels = channels;
		format.byte_format = AO_FMT_NATIVE;
		format.matrix = 0;
		dev = ao_open_live(driver, &format, NULL);

		/* decode and play */
		char *p =(char *)buffer;
		while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK)
			ao_play(dev, p, done);

		/* clean up */
		free(buffer);
		ao_close(dev);
		mpg123_close(mh);
		mpg123_delete(mh);
		mpg123_exit();
		ao_shutdown();
		}
		_exit(0);
	}

	/* Draw in loop */
	while (!glfwWindowShouldClose(window)) {

		if(panleft == 1 && screenleft >= -600 + 5){
			screenleft -= 5;
			screenright -= 5;
		}
		if(panright == 1 && screenright <= 600 - 5){
			screenleft += 5;
			screenright += 5;
		}
		if(panup == 1 && screentop >= -300 + 5){
			screentop -= 5;
			screenbotton -= 5;
		}
		if(pandown == 1 && screenbotton <= 300 - 5){
			screentop += 5;
			screenbotton += 5;
		}

		if(zoominstate == 1 && screenright-screenleft > 800) {
				screenleft /= 1.02;
				screenright /= 1.02;
				screentop /= 1.02;
				screenbotton /= 1.02;
		}
		if(zoomoutstate == 1 && screenright - screenleft < 1200) {
			if(screenleft >= -600.0f/1.02f)
				screenleft *= 1.02;
			if(screenright <= 600.0f/1.02f)
				screenright *= 1.02;
			if(screentop >= -300.0f/1.02f)
				screentop *= 1.02;
			if(screenbotton <= 300.0f/1.02f)
				screenbotton *= 1.02;
		}
		if(zoominstate == 1 || zoomoutstate == 1 || panleft == 1 || panright == 1 || panup == 1 || pandown == 1)
			reshapeWindow(window, width, height);
		// OpenGL Dramands
		draw();

		// Swap Frame Buffer in double buffering
		glfwSwapBuffers(window);

		// Poll for Keyboard and mouse events
		glfwPollEvents();

		// Control based on time (Time based transformation like 5 degrees rotation every 0.5s)
		current_time = glfwGetTime(); // Time in seconds
		if ((current_time - last_update_time) >= 0.5) { // atleast 0.5s elapsed since last frame
			// do something every 0.5 seconds ..
			last_update_time = current_time;
		}
	}

	glfwTerminate();
	exit(EXIT_SUCCESS);
}
