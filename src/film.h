#pragma once
// Owner: A. The timeline: turns eight separate scenes into one film.
//
// The film is a row of segments: scene 0, gust, scene 1, gust, ... scene 7.
// Each scene lasts kSceneLen[i] seconds; each gust between scenes lasts kGustLen[i].
#include "fb.h"

struct FilmTime {
    bool  gust;    // true while a transition is playing
    int   scene;   // the scene showing (during a gust: the scene being left)
    float t;       // seconds since that scene started
    float u;       // during a gust: progress 0..1 across the screen
};

float    filmLength();                      // whole film in seconds
FilmTime filmAt(float T);                   // what is on screen at time T
float    sceneStart(int scene);             // when a scene begins in the film
void     renderFilm(Framebuffer& out, float T);
