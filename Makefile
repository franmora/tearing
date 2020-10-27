

all: tearing

tearing: tearing.cpp
	g++ -DGLFW_INCLUDE_NONE -Iglad/include -o tearing glad/src/glad.cpp glad/src/glad_egl.cpp tearing.cpp -lglfw -lEGL

clean:
	rm -f tearing