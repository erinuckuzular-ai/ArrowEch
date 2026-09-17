#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

// Renders the editor to PNGs without a DAW: UISnapshot <outDir>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());

    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    auto* arrow = dynamic_cast<ArrowEchAudioProcessor*> (processor.get());

    auto findButton = [] (juce::Component& root, const juce::String& text) -> juce::Button*
    {
        std::function<juce::Button* (juce::Component&)> search = [&] (juce::Component& c) -> juce::Button*
        {
            if (auto* b = dynamic_cast<juce::Button*> (&c))
                if (b->getButtonText() == text)
                    return b;
            for (auto* child : c.getChildren())
                if (auto* found = search (*child))
                    return found;
            return nullptr;
        };
        return search (root);
    };

    auto snap = [&] (const juce::String& name, const juce::String& presetName, const juce::String& tab)
    {
        for (int i = 0; i < arrow->getNumPrograms(); ++i)
            if (arrow->getProgramName (i) == presetName)
                arrow->setCurrentProgram (i);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
        editor->setSize (1120, 866);
        if (auto* button = findButton (*editor, tab))
            button->triggerClick();
        for (int i = 0; i < 40; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5);

        std::printf ("%s: preset %d '%s'\n", name.toRawUTF8(), arrow->getCurrentProgram(), arrow->getProgramName (arrow->getCurrentProgram()).toRawUTF8());
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        outDir.getChildFile (name).deleteFile();
        juce::FileOutputStream out (outDir.getChildFile (name));
        juce::PNGImageFormat().writeImageToStream (image, out);
    };

    snap ("ui_garage.png", "Broken Toy Robot", "GLITCH GARAGE");
    snap ("ui_zoo.png", "Everything Everywhere (Sorry)", "TONE ZOO");
    return 0;
}
