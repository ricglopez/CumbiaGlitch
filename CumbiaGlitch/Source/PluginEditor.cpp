#include "PluginEditor.h"

namespace Palette
{
    const juce::Colour bg       { 0xff141217 };
    const juce::Colour panel    { 0xff1e1b22 };
    const juce::Colour panelHi  { 0xff2a2630 };
    const juce::Colour text     { 0xffede6da };
    const juce::Colour dim      { 0xff8e8696 };
    const juce::Colour cumbia   { 0xfff2a23a };   // canal A
    const juce::Colour beat     { 0xff3ac9d6 };   // canal B
    const juce::Colour glitch   { 0xffd94c8a };   // centro / efecto
}

static juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

static void setupKnobIn (juce::Component& parent, Knob& k, juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id, const juce::String& text, juce::Colour colour)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (uiFont (13.0f, true));
    k.label.setColour (juce::Label::textColourId, Palette::dim);
    parent.addAndMakeVisible (k.slider);
    parent.addAndMakeVisible (k.label);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, k.slider);
}

static void placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (18));
    k.slider.setBounds (area);
}

//==============================================================================
CumbiaLookAndFeel::CumbiaLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, Palette::panelHi);
    setColour (juce::ComboBox::outlineColourId, Palette::panelHi.brighter (0.2f));
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::ComboBox::arrowColourId, Palette::dim);
    setColour (juce::PopupMenu::backgroundColourId, Palette::panel);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::glitch);
    setColour (juce::ToggleButton::textColourId, Palette::text);
    setColour (juce::ToggleButton::tickColourId, Palette::text);
    setColour (juce::ToggleButton::tickDisabledColourId, Palette::dim);
    setColour (juce::TextButton::textColourOffId, Palette::text);
    setColour (juce::Label::textColourId, Palette::text);
}

void CumbiaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float thickness = juce::jmax (3.0f, radius * 0.16f);
    const float arcR = radius - thickness * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Palette::panelHi);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (colour);
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float innerR = arcR - thickness * 1.3f;
    g.setColour (Palette::panelHi.darker (0.3f));
    g.fillEllipse (centre.x - innerR, centre.y - innerR, innerR * 2.0f, innerR * 2.0f);

    const juce::Point<float> tip (centre.x + innerR * 0.8f * std::sin (angle),
                                  centre.y - innerR * 0.8f * std::cos (angle));
    g.setColour (Palette::text);
    g.drawLine ({ centre, tip }, 2.5f);
}

void CumbiaLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                                          float, float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, s);
        return;
    }

    const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
    const auto track = area.withSizeKeepingCentre (area.getWidth(), 8.0f);

    juce::ColourGradient grad (Palette::cumbia, track.getX(), 0.0f, Palette::beat, track.getRight(), 0.0f, false);
    grad.addColour (0.5, Palette::glitch);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (track, 4.0f);

    // marca del centro (los dos canales completos)
    g.setColour (Palette::text.withAlpha (0.6f));
    g.fillRect (juce::Rectangle<float> (2.0f, 22.0f).withCentre ({ area.getCentreX(), area.getCentreY() }));

    const auto thumb = juce::Rectangle<float> (26.0f, juce::jmin (44.0f, area.getHeight()))
                           .withCentre ({ sliderPos, area.getCentreY() });
    g.setColour (Palette::panelHi.brighter (0.15f));
    g.fillRoundedRectangle (thumb, 4.0f);
    g.setColour (Palette::text);
    g.drawRoundedRectangle (thumb, 4.0f, 1.5f);
    g.fillRect (juce::Rectangle<float> (thumb.getWidth() - 10.0f, 2.0f).withCentre (thumb.getCentre()));
}

void CumbiaLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                              bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = Palette::panelHi;
    if (over) c = c.brighter (0.15f);
    if (down) c = c.brighter (0.3f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (b.findColour (juce::TextButton::buttonOnColourId));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
}

//==============================================================================
WaveformView::WaveformView (CumbiaGlitchProcessor& p, int ch, juce::Colour c)
    : proc (p), channel (ch), accent (c)
{
    startParam = proc.apvts.getParameter (ch == 0 ? "aStart" : "bStart");
    endParam   = proc.apvts.getParameter (ch == 0 ? "aEnd" : "bEnd");
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void WaveformView::refresh()
{
    repaint();
}

void WaveformView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (Palette::bg);
    g.fillRoundedRectangle (r, 6.0f);

    auto sample = proc.getSample (channel);
    if (sample == nullptr || sample->peaks.empty())
    {
        g.setColour (Palette::dim);
        g.setFont (uiFont (13.0f));
        g.drawFittedText (juce::String::fromUTF8 ("Arrastra aqu\xc3\xad un audio\no pulsa \"Cargar sample\""),
                          getLocalBounds().reduced (8), juce::Justification::centred, 2);
        return;
    }

    float st = startParam->getValue(), en = endParam->getValue();
    if (st > en) std::swap (st, en);
    const float midY = r.getCentreY();
    const float halfH = r.getHeight() * 0.45f;
    const int bins = (int) sample->peaks.size() / 2;
    const int w = getWidth();

    for (int px = 0; px < w; ++px)
    {
        const float norm = (float) px / (float) w;
        const int b = juce::jlimit (0, bins - 1, (int) (norm * bins));
        const float mn = sample->peaks[(size_t) b * 2];
        const float mx = sample->peaks[(size_t) b * 2 + 1];
        const bool inRegion = norm >= st && norm <= en;
        g.setColour (inRegion ? accent : Palette::dim.withAlpha (0.35f));
        g.drawVerticalLine (px, midY - mx * halfH, midY - mn * halfH + 1.0f);
    }

    // zonas fuera de la seleccion
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (r.withRight (st * r.getWidth()));
    g.fillRect (r.withLeft (en * r.getWidth()));

    // marcadores de inicio y fin
    g.setColour (Palette::text);
    for (float m : { st, en })
    {
        const float mx = m * r.getWidth();
        g.drawLine (mx, 0.0f, mx, r.getHeight(), 2.0f);
        g.fillRect (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ mx, 5.0f }));
    }

    // cabezal de reproduccion
    const float ph = proc.getPlayhead (channel);
    if (ph >= 0.0f)
    {
        g.setColour (Palette::glitch);
        g.drawLine (ph * r.getWidth(), 0.0f, ph * r.getWidth(), r.getHeight(), 1.5f);
    }
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    const float norm = juce::jlimit (0.0f, 1.0f, (float) e.x / (float) juce::jmax (1, getWidth()));
    dragging = std::abs (norm - startParam->getValue()) <= std::abs (norm - endParam->getValue())
                   ? startParam : endParam;
    dragging->beginChangeGesture();
    dragging->setValueNotifyingHost (norm);
    repaint();
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == nullptr) return;
    dragging->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (float) e.x / (float) juce::jmax (1, getWidth())));
    repaint();
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    if (dragging != nullptr) dragging->endChangeGesture();
    dragging = nullptr;
}

//==============================================================================
ChannelPanel::ChannelPanel (CumbiaGlitchProcessor& p, int ch, juce::Colour c, const juce::String& t)
    : proc (p), channel (ch), accent (c), title (t), waveform (p, ch, c)
{
    const juce::String pre = ch == 0 ? "a" : "b";

    loadButton.setColour (juce::TextButton::buttonOnColourId, accent);
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Elige un sample", juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) loadFile (f);
                              });
    };
    addAndMakeVisible (loadButton);

    fileLabel.setFont (uiFont (12.0f));
    fileLabel.setColour (juce::Label::textColourId, Palette::dim);
    fileLabel.setText ("Sin sample", juce::dontSendNotification);
    addAndMakeVisible (fileLabel);

    addAndMakeVisible (waveform);

    setupKnobIn (*this, start, proc.apvts, pre + "Start", "Inicio", accent);
    setupKnobIn (*this, end, proc.apvts, pre + "End", "Fin", accent);
    setupKnobIn (*this, cutoff, proc.apvts, pre + "Cutoff", "Filtro lowpass", accent);

    bars.addItemList ({ juce::String::fromUTF8 ("1 comp\xc3\xa1s"), "2 compases", "4 compases", "8 compases", "16 compases" }, 1);
    addAndMakeVisible (bars);
    barsAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, pre + "Bars", bars);

    addAndMakeVisible (sync);
    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pre + "Sync", sync);
    sync.setTooltip ("Ajusta la velocidad para que la zona elegida dure los compases indicados");
}

void ChannelPanel::loadFile (const juce::File& f)
{
    if (! proc.loadSample (channel, f))
        fileLabel.setText ("No se pudo leer el archivo", juce::dontSendNotification);
    refresh();
}

bool ChannelPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a"))
            return true;
    return false;
}

void ChannelPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a"))
        {
            loadFile (juce::File (f));
            break;
        }
    repaint();
}

void ChannelPanel::refresh()
{
    const int v = proc.getSampleVersion (channel);
    if (v != knownVersion)
    {
        knownVersion = v;
        if (auto s = proc.getSample (channel))
            fileLabel.setText (s->name, juce::dontSendNotification);
    }
    waveform.refresh();
}

void ChannelPanel::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (dragOver ? accent : accent.withAlpha (0.25f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, dragOver ? 2.5f : 1.0f);

    g.setColour (accent);
    g.setFont (uiFont (18.0f, true));
    g.drawText (title, getLocalBounds().reduced (14, 0).removeFromTop (38), juce::Justification::centredLeft);
}

void ChannelPanel::resized()
{
    auto r = getLocalBounds().reduced (14, 0);
    r.removeFromTop (38);

    auto row = r.removeFromTop (30);
    loadButton.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (8);
    fileLabel.setBounds (row);

    r.removeFromTop (10);
    waveform.setBounds (r.removeFromTop (120));
    r.removeFromTop (12);

    auto knobs = r.removeFromTop (118);
    const int kw = knobs.getWidth() / 3;
    placeKnob (start, knobs.removeFromLeft (kw));
    placeKnob (end, knobs.removeFromLeft (kw));
    placeKnob (cutoff, knobs);

    r.removeFromTop (10);
    auto bottom = r.removeFromTop (28);
    bars.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2 - 4));
    bottom.removeFromLeft (12);
    sync.setBounds (bottom);
}

//==============================================================================
CumbiaGlitchEditor::CumbiaGlitchEditor (CumbiaGlitchProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      panelA (p, 0, Palette::cumbia, juce::String::fromUTF8 ("A  \xc2\xb7  CUMBIA")),
      panelB (p, 1, Palette::beat, juce::String::fromUTF8 ("B  \xc2\xb7  BEAT"))
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (panelA);
    addAndMakeVisible (panelB);

    setupKnob (wetDry, "wet", "DRY  /  WET", Palette::glitch);
    setupKnob (chaos, "chaos", "Caos", Palette::glitch);
    setupKnob (texture, "texture", "Textura", Palette::glitch);
    setupKnob (gain, "gain", "Volumen", Palette::text);
    wetDry.label.setFont (uiFont (14.0f, true));
    wetDry.label.setColour (juce::Label::textColourId, Palette::text);
    chaos.slider.setTooltip ("Cuantos golpes se rompen: troceo, repeticiones, granulado y silencios");
    texture.slider.setTooltip ("Que tan sucio y abstracto: bitcrush, granos y modulacion metalica");

    mode.addItemList ({ "Loop (sigue al DAW)", "MIDI (con teclas)" }, 1);
    addAndMakeVisible (mode);
    modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "mode", mode);
    modeLabel.setText ("Modo", juce::dontSendNotification);
    modeLabel.setFont (uiFont (13.0f, true));
    modeLabel.setColour (juce::Label::textColourId, Palette::dim);
    modeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (modeLabel);

    xfade.setSliderStyle (juce::Slider::LinearHorizontal);
    xfade.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    xfade.setDoubleClickReturnValue (true, 0.5);
    addAndMakeVisible (xfade);
    xfadeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "xfade", xfade);

    setSize (960, 600);
    startTimerHz (30);
}

CumbiaGlitchEditor::~CumbiaGlitchEditor()
{
    setLookAndFeel (nullptr);
}

void CumbiaGlitchEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& text, juce::Colour c)
{
    setupKnobIn (*this, k, proc.apvts, id, text, c);
}

void CumbiaGlitchEditor::timerCallback()
{
    panelA.refresh();
    panelB.refresh();
}

void CumbiaGlitchEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::bg);

    auto header = getLocalBounds().removeFromTop (56).reduced (20, 0);
    g.setFont (uiFont (26.0f, true));
    g.setColour (Palette::text);
    g.drawText ("CUMBIA", header, juce::Justification::centredLeft);
    const int w1 = juce::GlyphArrangement::getStringWidthInt (uiFont (26.0f, true), "CUMBIA");
    g.setColour (Palette::glitch);
    g.drawText ("GLITCH", header.withTrimmedLeft (w1 + 2), juce::Justification::centredLeft);
    g.setFont (uiFont (13.0f));
    g.setColour (Palette::dim);
    g.drawText (juce::String::fromUTF8 ("sampler de dos canales  \xc2\xb7  cumbia \xc3\x97 IDM"), header, juce::Justification::centredRight);

    // panel central
    const auto centre = juce::Rectangle<int> (362, 64, 236, 420).toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (centre, 10.0f);
    g.setColour (Palette::glitch.withAlpha (0.25f));
    g.drawRoundedRectangle (centre.reduced (0.5f), 10.0f, 1.0f);

    // zona del crossfader
    auto xf = juce::Rectangle<int> (16, 498, 928, 86);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (xf.toFloat(), 10.0f);
    auto labels = xf.reduced (20, 8).removeFromTop (18);
    g.setFont (uiFont (13.0f, true));
    g.setColour (Palette::cumbia);
    g.drawText ("A", labels, juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.drawText ("A + B", labels, juce::Justification::centred);
    g.setColour (Palette::beat);
    g.drawText ("B", labels, juce::Justification::centredRight);
}

void CumbiaGlitchEditor::resized()
{
    panelA.setBounds (16, 64, 330, 420);
    panelB.setBounds (614, 64, 330, 420);

    auto c = juce::Rectangle<int> (362, 64, 236, 420).reduced (14, 12);
    modeLabel.setBounds (c.removeFromTop (18));
    mode.setBounds (c.removeFromTop (28));
    c.removeFromTop (10);
    placeKnob (wetDry, c.removeFromTop (140));
    c.removeFromTop (6);
    auto row = c.removeFromTop (104);
    placeKnob (chaos, row.removeFromLeft (row.getWidth() / 2));
    placeKnob (texture, row);
    c.removeFromTop (6);
    placeKnob (gain, c.withSizeKeepingCentre (110, c.getHeight()));

    xfade.setBounds (juce::Rectangle<int> (16, 498, 928, 86).reduced (24, 0).withTrimmedTop (28).withTrimmedBottom (10));
}
