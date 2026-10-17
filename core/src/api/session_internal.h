// Internal (non-C) access to sessions, for model backends and tests. Not part of the public API.
#ifndef MUTQIN_API_SESSION_INTERNAL_H
#define MUTQIN_API_SESSION_INTERNAL_H

#include <memory>
#include <string>
#include <vector>

#include "inference/acoustic_model.h"
#include "inference/token_recognizer.h"
#include "inference/units.h"
#include "mutqin.h"

namespace mutqin {

// Gives the session its recognition model; mq_session_push_audio needs one.
void SetSessionModel(mq_session* session, std::unique_ptr<AcousticModel> model);

// Gives the session a recognizer that returns tokens directly (sherpa-onnx style).
void SetSessionRecognizer(mq_session* session, std::unique_ptr<TokenRecognizer> recognizer);

// Gives an automatic session (mq_session_create_auto) what a model manifest with units gives it: the groups
// of the whole Quran to find the place in, and the table to build the range from once it is found.
void SetSessionAutoUnits(mq_session* session, const std::vector<UnitGroup>& all_groups,
                         const std::string& units_path, int collapse_runs);

}  // namespace mutqin

#endif  // MUTQIN_API_SESSION_INTERNAL_H
