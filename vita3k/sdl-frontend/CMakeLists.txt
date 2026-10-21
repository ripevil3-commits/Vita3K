if(VITA3K_PS4)
	set(SDL_FRONTEND_FRAME_HOST_SOURCE src/frame_host_ps4.cpp)
else()
	set(SDL_FRONTEND_FRAME_HOST_SOURCE src/frame_host.cpp)
endif()

add_library(
	sdl-frontend
	STATIC
	include/sdl-frontend/session.h
	${SDL_FRONTEND_FRAME_HOST_SOURCE}
	src/frame_host.h
	src/session.cpp
)

target_include_directories(sdl-frontend PUBLIC include)
target_link_libraries(sdl-frontend PUBLIC emuenv)
target_link_libraries(sdl-frontend PRIVATE app audio config ctrl dialog ime motion renderer touch util SDL3::SDL3)
