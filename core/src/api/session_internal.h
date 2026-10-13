// Internal (non-C) access to sessions, for model backends and tests. Not part of the public API.
#ifndef MUTQIN_API_SESSION_INTERNAL_H
#define MUTQIN_API_SESSION_INTERNAL_H

#include <memory>

#include "inference/acoustic_model.h"
#include "mutqin.h"

namespace mutqin {

// Gives the session its recognition model; mq_session_push_audio needs one.
void SetSessionModel(mq_session* session, std::unique_ptr<AcousticModel> model);

}  // namespace mutqin

#endif  // MUTQIN_API_SESSION_INTERNAL_H
