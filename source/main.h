#ifndef MAIN_H__
#define MAIN_H__

#include "c4d_basedocument.h"
#include "c4d.h"

Bool InitVDBLib(void); // defined in C4DOpenVDBFunctions.cpp
Bool RegisterC4DOpenVDBPrimitive(void);
Bool RegisterC4DOpenVDBHelp(void);
Bool RegisterC4DOpenVDBVisualizer(void);
Bool RegisterC4DOpenVDBMesher(void);
Bool RegisterC4DOpenVDBCombine(void);
Bool RegisterC4DOpenVDBFromPolygons(void);

#endif // MAIN_H__
