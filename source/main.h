#ifndef MAIN_H__
#define MAIN_H__

#include "c4d_basedocument.h"
#include "c4d.h"
#include "c4d_particles.h"

Bool InitVDBLib(void); // defined in C4DOpenVDBFunctions.cpp
Bool RegisterC4DOpenVDBPrimitive(void);
Bool RegisterC4DOpenVDBHelp(void);
Bool RegisterC4DOpenVDBVisualizer(void);
Bool RegisterC4DOpenVDBToPolygons(void);
Bool RegisterC4DOpenVDBCombine(void);
Bool RegisterC4DOpenVDBFromPolygons(void);
Bool RegisterC4DOpenVDBSmooth(void);
Bool RegisterC4DOpenVDBReshape(void);
Bool RegisterC4DOpenVDBFromParticles(void);

#endif // MAIN_H__
