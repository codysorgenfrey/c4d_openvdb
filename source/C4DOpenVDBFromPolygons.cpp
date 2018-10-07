//
//  C4DOpenVDBFromPolygons.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 10/6/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBFromPolygons.h"
#include "C4DOpenVDBFunctions.h" // to get our vdb functions
#include "C4DOpenVDBHelp.h" // to launch help


//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBFromPolygons Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBFromPolygons::Init(GeListNode* node)
{
    if (!SUPER::Init(node)) return false;
    return true;
}

void C4DOpenVDBFromPolygons::Free(GeListNode *node)
{
    SUPER::Free(node);
}

static Bool C4DOpenVDBFromPolygonsObjectHelpDelegate(const String& opType, const String& baseType, const String& group, const String& property)
{
    // Make sure that your object type name is unique and only return true if this is really your object type name and you are able to present a decent help.
    if (opType.ToUpper() == "OC4DOPENVDBFROMPOLYGONS")
    {
        if (property != opType) HelpDialog::location = (opType.ToUpper() + ".html#" + property).GetCStringCopy();
        else if (group != opType) HelpDialog::location = (opType.ToUpper() + ".html#" + group).GetCStringCopy();
        else HelpDialog::location = (opType.ToUpper() + ".html").GetCStringCopy();
        CallCommand(Oc4dopenvdbhelp);
        return true;
    }
    // If this is not your object/plugin type return false.
    return false;
}


Bool RegisterC4DOpenVDBFromPolygons(void)
{
    if (!RegisterPluginHelpDelegate(Oc4dopenvdbfrompolygons, C4DOpenVDBFromPolygonsObjectHelpDelegate)) return false;
    
    return RegisterObjectPlugin(Oc4dopenvdbfrompolygons,
                                GeLoadString(IDS_C4DOpenVDBFromPolygons),
                                OBJECT_GENERATOR | OBJECT_INPUT,
                                C4DOpenVDBFromPolygons::Alloc,
                                "Oc4dopenvdbfrompolygons",
                                AutoBitmap("C4DOpenVDB.tiff"),
                                0);
}
