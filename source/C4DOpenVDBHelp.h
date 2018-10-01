//
//  C4DOpenVDBHelp.hpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 8/29/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#ifndef C4DOpenVDBHelp_h
#define C4DOpenVDBHelp_h

#include "C4DOpenVDBObject.h"
#include <string>

class HelpDialog : public GeDialog
{
    INSTANCEOF(HelpDialog, GeDialog);
public:
    static std::string location;
    virtual Bool CreateLayout(void);
    virtual Bool InitValues(void);
};

class C4DOpenVDBHelp : public CommandData
{
    INSTANCEOF(C4DOpenVDBHelp, CommandData);
private:
    HelpDialog _dialog;
public:
    virtual Bool Execute(BaseDocument *doc);
    virtual Bool RestoreLayout(void *secret);
    static C4DOpenVDBHelp* Alloc() { return NewObjClear( C4DOpenVDBHelp ); }
};


#endif /* C4DOpenVDBHelp_h */
