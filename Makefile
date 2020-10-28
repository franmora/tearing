

all: tearing

tearing:
	arm-linux-gnueabihf-g++ -DGLFW_INCLUDE_NONE -Iglad/include -o tearing glad/src/glad.cpp glad/src/glad_egl.cpp main.cpp video.cpp -lglfw -lEGL

clean:
	rm -f tearing
