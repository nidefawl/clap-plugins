#include "../container-of.hh"
#include "../merge-process-status.hh"

#include "../container-of.hh"
#include "../parameter.hh"
#include "voice-expander-module.hh"
#include "voice-module.hh"

namespace clap {

   VoiceExpanderModule::VoiceExpanderModule(CorePlugin &plugin,
                                            uint32_t moduleId,
                                            std::unique_ptr<Module> module,
                                            uint32_t channelCount)
      : Module(plugin, "voice-expander", moduleId), _outputBuffer(channelCount, BLOCK_SIZE) {

      _voices[0] = std::make_unique<VoiceModule>(_plugin, std::move(module), 1);
      for (uint32_t i = 1; i < _voices.size(); ++i) {
         _voices[i] = std::make_unique<VoiceModule>(*_voices[0]);
         _voices[i]->setVoiceIndex(i);
      }

      for (auto &voice : _voices)
         _sleepingVoices.pushBack(&voice->_stateHook);
   }

   bool VoiceExpanderModule::doActivate(double sampleRate, uint32_t maxFrameCount) {
      for (auto &voice : _voices) {
         if (voice) {
            if (!voice->activate(sampleRate, maxFrameCount)) {
               deactivate();
               return false;
            }
         }
      }
      return true;
   }

   void VoiceExpanderModule::doDeactivate() {
      for (auto &voice : _voices)
         if (voice)
            voice->deactivate();
   }

   clap_process_status VoiceExpanderModule::process(const Context &c, uint32_t numFrames) noexcept {
      clap_process_status status = CLAP_PROCESS_SLEEP;

      _outputBuffer.clear(0);

      for (auto it = _activeVoices.begin(); !it.end();) {
         auto voice = containerOf(it.item(), &VoiceModule::_stateHook);
         status = mergeProcessStatus(status, voice->process(c, numFrames));
         ++it;

         _outputBuffer.sum(_outputBuffer, voice->_outputBuffer, numFrames);

         if (status == CLAP_PROCESS_SLEEP)
            releaseVoice(*voice);
      }

      return status;
   }

   VoiceModule *VoiceExpanderModule::findActiveVoice(int32_t key, int32_t channel) const {
      for (auto it = _activeVoices.begin(); !it.end(); ++it) {
         auto voice = containerOf(it.item(), &VoiceModule::_stateHook);
         assert(voice->isAssigned());
         if (voice->match(key, channel))
            return voice;
      }

      return nullptr;
   }

   VoiceModule *VoiceExpanderModule::assignVoice() {
      if (_sleepingVoices.empty())
         return nullptr; // TODO: steal voice instead

      auto voice = containerOf(_sleepingVoices.front(), &VoiceModule::_stateHook);
      assert(!voice->isAssigned());
      voice->_stateHook.unlink();
      voice->assign();
      _activeVoices.pushBack(&voice->_stateHook);
      return voice;
   }

   void VoiceExpanderModule::releaseVoice(VoiceModule &voice) {
      assert(voice.isAssigned());
      voice._stateHook.unlink();
      voice._isAssigned = false;
      _sleepingVoices.pushBack(&voice._stateHook);

      while (!voice._parametersToReset.empty()) {
         auto paramVoice =
            containerOf(voice._parametersToReset.front(), &Parameter::Voice::_resetHook);
         paramVoice->_hasValue = false;
         paramVoice->_hasModulation = false;
         paramVoice->_hasModulatedValue = false;
         paramVoice->_resetHook.unlink();
         paramVoice->_valueToProcessHook.unlink();
         paramVoice->_modulationToProcessHook.unlink();
         paramVoice->_modulatedValueToProcessHook.unlink();
      }
   }

   bool VoiceExpanderModule::wantsNoteEvents() const noexcept { return true; }

   void VoiceExpanderModule::onNoteOn(const clap_event_note &note) noexcept {
      auto voice = findActiveVoice(note.key, note.channel);
      if (voice) {
         voice->onNoteOn(note);
         return;
      }

      voice = assignVoice();
      if (!voice)
         return;

      voice->onNoteOn(note);
   }

   void VoiceExpanderModule::onNoteOff(const clap_event_note &note) noexcept {
      auto voice = findActiveVoice(note.key, note.channel);
      if (voice)
         voice->onNoteOff(note);
   }

   void VoiceExpanderModule::onNoteChoke(const clap_event_note &note) noexcept {
      auto voice = findActiveVoice(note.key, note.channel);
      if (voice)
         voice->onNoteChoke(note);
   }

   void VoiceExpanderModule::onNoteExpression(const clap_event_note_expression &noteExp) noexcept {
      auto voice = findActiveVoice(noteExp.key, noteExp.channel);
      if (voice)
         voice->onNoteExpression(noteExp);
   }
} // namespace clap
