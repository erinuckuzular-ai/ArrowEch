#pragma once

#include "ArrowLookAndFeel.h"

// The mascot. Bounces with the output level, blinks, follows the mouse,
// gets dizzy when feedback runs away and wears shades when the twins are on.
class Blob : public juce::Component
{
public:
    std::function<void()> onPoke;

    void update (float newLevel, bool newDizzy, bool newShades)
    {
        level = juce::jmax (newLevel, level * 0.8f);
        dizzy = newDizzy;
        shades = newShades;
        spin += 0.25f;
        squish *= 0.85f;

        if (--blinkCountdown <= 0)
        {
            blinkFrames = 5;
            blinkCountdown = 60 + juce::Random::getSystemRandom().nextInt (150);
        }
        if (blinkFrames > 0)
            --blinkFrames;

        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        squish = 1.0f;
        if (onPoke)
            onPoke();
    }

    void paint (juce::Graphics& g) override
    {
        using LAF = ArrowLookAndFeel;
        const auto area = getLocalBounds().toFloat().reduced (8.0f);
        const float bounce = juce::jlimit (0.0f, 1.0f, level * 1.4f);
        const float stretchY = 1.0f + 0.14f * bounce - 0.2f * squish;
        const float stretchX = 1.0f - 0.07f * bounce + 0.18f * squish;

        const float w = area.getWidth() * 0.9f * stretchX;
        const float h = area.getHeight() * 0.85f * stretchY;
        const auto body = juce::Rectangle<float> (w, h).withCentre ({ area.getCentreX(), area.getBottom() - h * 0.5f - 2.0f });

        // Body
        g.setColour (LAF::ink);
        g.fillEllipse (body.translated (4.0f, 4.0f));
        g.setColour (dizzy ? LAF::tomato : LAF::butter);
        g.fillEllipse (body);
        g.setColour (LAF::ink);
        g.drawEllipse (body, 3.0f);

        // Cheeks
        g.setColour (LAF::bubblegum.withAlpha (0.8f));
        const float cheek = w * 0.14f;
        g.fillEllipse (juce::Rectangle<float> (cheek, cheek * 0.6f).withCentre ({ body.getX() + w * 0.2f, body.getCentreY() + h * 0.12f }));
        g.fillEllipse (juce::Rectangle<float> (cheek, cheek * 0.6f).withCentre ({ body.getRight() - w * 0.2f, body.getCentreY() + h * 0.12f }));

        // Eyes
        const float eyeSize = w * 0.26f;
        const juce::Point<float> eyes[] = { { body.getCentreX() - w * 0.17f, body.getY() + h * 0.38f },
                                            { body.getCentreX() + w * 0.17f, body.getY() + h * 0.38f } };
        const auto mouse = getMouseXYRelative().toFloat();

        for (auto eye : eyes)
        {
            const auto eyeRect = juce::Rectangle<float> (eyeSize, eyeSize).withCentre (eye);
            g.setColour (LAF::ink);

            if (blinkFrames > 0 && ! dizzy)
            {
                g.drawLine (eyeRect.getX(), eye.y, eyeRect.getRight(), eye.y, 3.0f);
                continue;
            }

            g.setColour (LAF::white);
            g.fillEllipse (eyeRect);
            g.setColour (LAF::ink);
            g.drawEllipse (eyeRect, 2.5f);

            if (dizzy)
            {
                juce::Path spiral;
                for (int i = 0; i < 40; ++i)
                {
                    const float t = (float) i / 40.0f;
                    const auto pt = eye.getPointOnCircumference (t * eyeSize * 0.4f, spin + t * 12.0f);
                    if (i == 0) spiral.startNewSubPath (pt); else spiral.lineTo (pt);
                }
                g.strokePath (spiral, juce::PathStrokeType (2.0f));
            }
            else
            {
                auto look = mouse - eye;
                const float maxOffset = eyeSize * 0.2f;
                if (look.getDistanceFromOrigin() > maxOffset)
                    look = look * (maxOffset / look.getDistanceFromOrigin());

                const float pupil = eyeSize * (0.42f + 0.15f * bounce);
                g.fillEllipse (juce::Rectangle<float> (pupil, pupil).withCentre (eye + look));
                g.setColour (LAF::white);
                g.fillEllipse (juce::Rectangle<float> (pupil * 0.3f, pupil * 0.3f).withCentre (eye + look - juce::Point<float> (pupil * 0.15f, pupil * 0.15f)));
            }
        }

        // Shades
        if (shades)
        {
            g.setColour (LAF::ink);
            for (auto eye : eyes)
                g.fillRoundedRectangle (juce::Rectangle<float> (eyeSize * 1.3f, eyeSize * 0.8f).withCentre (eye.translated (0.0f, 2.0f)), 4.0f);
            g.drawLine (eyes[0].x, eyes[0].y, eyes[1].x, eyes[1].y, 3.0f);
        }

        // Mouth: smiles when quiet, opens wide when loud.
        const auto mouthCentre = juce::Point<float> (body.getCentreX(), body.getY() + h * 0.68f);
        g.setColour (LAF::ink);
        if (bounce > 0.05f)
        {
            auto mouth = juce::Rectangle<float> (w * (0.2f + 0.1f * bounce), h * (0.06f + 0.2f * bounce)).withCentre (mouthCentre);
            g.fillEllipse (mouth);
            g.setColour (LAF::tomato);
            g.fillEllipse (mouth.removeFromBottom (mouth.getHeight() * 0.45f).reduced (mouth.getWidth() * 0.2f, 0.0f));
        }
        else
        {
            juce::Path smile;
            smile.addCentredArc (mouthCentre.x, mouthCentre.y - h * 0.06f, w * 0.12f, h * 0.08f, 0.0f,
                                 juce::MathConstants<float>::pi * 0.65f, juce::MathConstants<float>::pi * 1.35f, true);
            g.strokePath (smile, { 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        }
    }

private:
    float level = 0.0f, spin = 0.0f, squish = 0.0f;
    bool dizzy = false, shades = false;
    int blinkCountdown = 90, blinkFrames = 0;
};
