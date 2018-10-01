//
//  C4DOpenVDBHelp.cpp
//  C4DOpenVDB
//
//  Created by Cody Sorgenfrey on 8/29/18.
//  Copyright © 2018 MAXON Computer GmbH. All rights reserved.
//

#include "C4DOpenVDBHelp.h"
#include "customgui_htmlviewer.h" // for help viewer

//---------------------------------------------------------------------------------
//
// MARK: HelpDialog Class
//
//---------------------------------------------------------------------------------

std::string HelpDialog::location = "index.html";

Bool HelpDialog::CreateLayout(void)
{
    if (!GeDialog::CreateLayout()) return false;
    
    SetTitle(GeLoadString(IDS_C4DOpenVDBHelp));
    AddCustomGui(C4DOPENVDB_HELP_HTML, CUSTOMGUI_HTMLVIEWER, String(), BFH_SCALEFIT | BFV_SCALEFIT, 300, 300, BaseContainer());
    return true;
}

Bool HelpDialog::InitValues(void)
{
    void *customGUI = FindCustomGui(C4DOPENVDB_HELP_HTML, CUSTOMGUI_HTMLVIEWER);
    HtmlViewerCustomGui *myHtml = static_cast<HtmlViewerCustomGui*>(customGUI);
    myHtml->SetUrl(GeGetPluginPath().GetString()+ "/help/" + String(location.c_str()), URL_ENCODING_UTF16);
    return true;
}

//---------------------------------------------------------------------------------
//
// MARK: C4DOpenVDBHelp Class
//
//---------------------------------------------------------------------------------

Bool C4DOpenVDBHelp::Execute(BaseDocument *doc)
{
    if (_dialog.IsOpen() == false)
        _dialog.Open(DLG_TYPE_ASYNC, Oc4dopenvdbhelp, -1, -1, 500, 500);
    return true;
}

Bool C4DOpenVDBHelp::RestoreLayout(void *secret)
{
    return _dialog.RestoreLayout(Oc4dopenvdbhelp, 0, secret);
}

Bool RegisterC4DOpenVDBHelp(void)
{
    return RegisterCommandPlugin(Oc4dopenvdbhelp,
                                 GeLoadString(IDS_C4DOpenVDBHelp),
                                 0,
                                 AutoBitmap("C4DOpenVDB.tiff"),
                                 "OpenVDB Help Documentation",
                                 C4DOpenVDBHelp::Alloc());
}
