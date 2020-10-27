

all: tearing

tearing: tearing.cpp
	g++ -DGLFW_INCLUDE_NONE -Iglad/include -lglfw -lEGL -o tearing glad/src/glad.cpp glad/src/glad_egl.cpp tearing.cpp

clean:
	rm -f tearing