CONTAINER Oc4dopenvdbreshape {
    INCLUDE Obase;
    NAME Oc4dopenvdbreshape;
    GROUP ID_OBJECTPROPERTIES {
        LONG C4DOPENVDB_RESHAPE_OP {
            CYCLE {
                C4DOPENVDB_RESHAPE_OP_DILATE;
                C4DOPENVDB_RESHAPE_OP_ERODE;
                C4DOPENVDB_RESHAPE_OP_OPEN;
                C4DOPENVDB_RESHAPE_OP_CLOSE;
            }
        }
        LONG C4DOPENVDB_RESHAPE_OFFSET { DEFAULT 1; MIN 1; STEP 1; }
        LONG C4DOPENVDB_RESHAPE_RENORM {
            CYCLE {
                C4DOPENVDB_RESHAPE_RENORM_FIRST;
                C4DOPENVDB_RESHAPE_RENORM_SEC;
                C4DOPENVDB_RESHAPE_RENORM_FIFTH;
            }
        }
    }
}
