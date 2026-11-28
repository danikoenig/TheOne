#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Fester Rahmen, derselbe bei allen Plugins von Dani Koenig: 4 px dick (bei Grundgroesse), orange.
// Oben eckig (Titelleiste des Hosts), unten mit dem Eckenradius der macOS-Fenster gerundet,
// damit der Rahmen die Fensterecken nahtlos nachzeichnet. Liegt als oberstes Kind ueber dem Editor.
class FrameOverlay : public juce::Component
{
public:
    FrameOverlay() { setInterceptsMouseClicks (false, false); setOpaque (false); }

    // baseWidth = Fensterbreite bei 100 %. 0 = Fenster skaliert nicht, Rahmen bleibt 4 px
    void attach (juce::Component& parent, float baseWidth)
    {
        if (getParentComponent() != &parent)
            parent.addAndMakeVisible (*this);
        scale = baseWidth > 0.0f ? juce::jmax (0.3f, (float) parent.getWidth() / baseWidth) : 1.0f;
        setBounds (parent.getLocalBounds());
        toFront (false);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const float fw = 4.0f * scale, cr = 10.0f * scale;
        const auto full = getLocalBounds().toFloat();
        juce::Path ring;
        ring.addRoundedRectangle (full.getX(), full.getY(), full.getWidth(), full.getHeight(), cr, cr, false, false, true, true);
        ring.addRoundedRectangle (full.getX() + fw, full.getY() + fw, full.getWidth() - 2 * fw, full.getHeight() - 2 * fw,
                                  juce::jmax (2.0f, cr - fw), juce::jmax (2.0f, cr - fw), false, false, true, true);
        ring.setUsingNonZeroWinding (false);
        g.setColour (juce::Colour (0xffff7a2f));
        g.fillPath (ring);
    }

private:
    float scale = 1.0f;
};
