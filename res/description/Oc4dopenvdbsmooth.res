CONTAINER Oc4dopenvdbsmooth {
    INCLUDE Obase;
    NAME Oc4dopenvdbsmooth;
    GROUP ID_OBJECTPROPERTIES {
        LONG C4DOPENVDB_SMOOTH_OP {
            CYCLE {
                C4DOPENVDB_SMOOTH_OP_MEAN;
                C4DOPENVDB_SMOOTH_OP_GAUS;
                C4DOPENVDB_SMOOTH_OP_MEAD;
                C4DOPENVDB_SMOOTH_OP_MEANFLOW;
                C4DOPENVDB_SMOOTH_OP_LAP;
            }
        }
        LONG C4DOPENVDB_SMOOTH_FILTER { DEFAULT 1; MIN 1; STEP 1; }
        LONG C4DOPENVDB_SMOOTH_ITER { DEFAULT 1; MIN 1; STEP 1; }
        LONG C4DOPENVDB_SMOOTH_RENORM {
            CYCLE {
                C4DOPENVDB_SMOOTH_RENORM_FIRST;
                C4DOPENVDB_SMOOTH_RENORM_SEC;
                C4DOPENVDB_SMOOTH_RENORM_FIFTH;
            }
        }
    }
}
