CONTAINER Oc4dopenvdbvisualizer {
	INCLUDE Obase;
    NAME Oc4dopenvdbvisualizer;
	GROUP ID_OBJECTPROPERTIES {
        LINK C4DOPENVDB_VIS_OBJECT {  }
        INCLUDE Oprimitiveaxis;
        REAL C4DOPENVDB_VIS_OFFSET { UNIT METER; STEP 1.0; MINSLIDER -100.0; MAXSLIDER 100.0; CUSTOMGUI REALSLIDER; }
        GRADIENT C4DOPENVDB_VIS_COLOR {  }
    }
}
