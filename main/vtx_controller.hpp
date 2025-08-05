#ifndef VTX_CONTROLLER_HPP
#define VTX_CONTROLLER_HPP

enum VtxSequenceType {
    SEQ_FULL_SETUP,
};

void run_vtx_sequence(VtxSequenceType seq_type);

#endif