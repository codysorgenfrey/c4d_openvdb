//
//  C4DOpenVDBGenerator.h
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 9/14/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#ifndef C4DOpenVDBGenerator_h
#define C4DOpenVDBGenerator_h

#include "main.h"
#include "c4d_symbols.h" // Enum of string constants for the plugin
#include "Oc4dopenvdbprimitive.h" // Enum of ID number constants for UI elements
#include "Oc4dopenvdbhelp.h" // Enum of ID number constants for UI elements
#include "Oc4dopenvdbvisualizer.h" // Enum of ID number constants for UI elements
#include "Oc4dopenvdbmesher.h" // Enum of ID number constants for UI elements
#include "Oc4dopenvdbcombine.h" // Enum of ID number constants for UI elements
#include "c4d_gl.h" //For drawing with OpenGL in c4d

class C4DOpenVDBGenerator : public ObjectData
{
    INSTANCEOF(C4DOpenVDBGenerator, ObjectData);
public:
    maxon::BaseArray<BaseLink*> *slaveLinks;
    
    static NodeData* Alloc(void) { return NewObjClear(C4DOpenVDBGenerator); };
    virtual Bool Init(GeListNode* node);
    virtual void Free(GeListNode* node);
    virtual DRAWRESULT Draw(BaseObject *op, DRAWPASS drawpass, BaseDraw *bd, BaseDrawHelp *bh);
    Bool RegisterSlave(BaseObject *slave);
    Bool RegisterSlaves(BaseObject *first, Bool topLevelOnly);
    void FreeSlaves(BaseDocument *doc);
    Bool RecurseCollectVDBs(BaseObject *obj, maxon::BaseArray<BaseObject*> *inputs, Bool topLevelOnly);
private:
    Bool RecurseSetSlave(BaseObject *obj, Bool topLevelOnly);
};

// Public helper functions
Bool ValidVDBType(Int32 type);

#endif /* C4DOpenVDBGenerator_h */
