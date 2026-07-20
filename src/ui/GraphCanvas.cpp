#include "GraphCanvas.h"

using namespace juce;

namespace
{
constexpr float kCardW = 74.0f, kCardH = 22.0f;
constexpr float kBusW = 22.0f;
} // namespace

GraphCanvas::GraphCanvas (ChoraleProcessor& p, std::function<void (int)> cb)
    : proc (p), onSelect (std::move (cb))
{
    startTimerHz (10); // power dots / external edits
}

void GraphCanvas::setSelected (int nodeId)
{
    selected = nodeId;
    repaint();
}

String GraphCanvas::nodeName (int id)
{
    using namespace graph;
    switch (kindOf (id))
    {
        case NodeKind::Voice: return "V" + String (id - kVoice0 + 1);
        case NodeKind::Eq: return "EQ" + String (id - kEq0 + 1);
        case NodeKind::Comp: return "CMP" + String (id - kComp0 + 1);
        case NodeKind::Sat: return "SAT" + String (id - kSat0 + 1);
        case NodeKind::Gain: return "GN" + String (id - kGain0 + 1);
        case NodeKind::Echo: return "ECH" + String (id - kEcho0 + 1);
        case NodeKind::Verb: return "VRB" + String (id - kVerb0 + 1);
        case NodeKind::Out: return "OUT";
    }
    return {};
}

String GraphCanvas::powerParam (int id)
{
    using namespace graph;
    const auto v = String (id % 8 + 1);
    switch (kindOf (id))
    {
        case NodeKind::Eq: return "v" + v + "EqOn";
        case NodeKind::Comp: return "v" + v + "CompOn";
        case NodeKind::Sat: return "v" + v + "SatOn";
        default: return {};
    }
}

bool GraphCanvas::nodeVisible (int id) const
{
    using namespace graph;
    if (kindOf (id) == NodeKind::Voice || id == kOut)
        return true;
    return proc.nodeOnCanvas (id);
}

Point<int> GraphCanvas::defaultPos (int id) const
{
    using namespace graph;
    const int slot = id % 8;
    const int rowY = 8 + slot * ((getHeight() - 38) / 8 > 0 ? (getHeight() - 38) / 8 : 24);
    switch (kindOf (id))
    {
        case NodeKind::Voice: return { 12, rowY };
        case NodeKind::Eq: return { 140, rowY };
        case NodeKind::Comp: return { 260, rowY };
        case NodeKind::Sat: return { 380, rowY };
        case NodeKind::Gain: return { 520, 30 + (id - kGain0) * 40 };
        case NodeKind::Echo: return { 620, 40 + (id - kEcho0) * 44 };
        case NodeKind::Verb: return { 620, 140 + (id - kVerb0) * 44 };
        case NodeKind::Out: return { 0, 0 }; // bus bar, position fixed
    }
    return { 12, 12 };
}

Rectangle<float> GraphCanvas::nodeRect (int id) const
{
    if (id == graph::kOut) // vertical bus bar on the right edge
        return { (float) getWidth() - kBusW - 8.0f, 8.0f, kBusW, (float) getHeight() - 16.0f };
    const auto p = proc.nodePos (id, defaultPos (id));
    return { (float) p.x, (float) p.y, kCardW, kCardH };
}

Point<float> GraphCanvas::outPort (int id) const
{
    const auto r = nodeRect (id);
    return { r.getRight(), r.getCentreY() };
}

Point<float> GraphCanvas::inPort (int id) const
{
    const auto r = nodeRect (id);
    return { r.getX(), r.getCentreY() };
}

// OUT is a bus bar: cables land at their source's height, so eight voices
// arrive as parallel lines instead of a knot.
Point<float> GraphCanvas::inPortFor (int id, float sourceY) const
{
    const auto r = nodeRect (id);
    if (id != graph::kOut)
        return { r.getX(), r.getCentreY() };
    return { r.getX(), jlimit (r.getY() + 8.0f, r.getBottom() - 8.0f, sourceY) };
}

Path GraphCanvas::cablePath (Point<float> a, Point<float> b) const
{
    Path p;
    const float dx = jmax (18.0f, std::abs (b.x - a.x) * 0.4f);
    p.startNewSubPath (a);
    p.cubicTo (a.x + dx, a.y, b.x - dx, b.y, b.x, b.y);
    return p;
}

int GraphCanvas::nodeAt (Point<float> pos) const
{
    for (int id = graph::kNumNodes; --id >= 0;)
        if (nodeVisible (id) && nodeRect (id).expanded (4.0f).contains (pos))
            return id;
    return -1;
}

bool GraphCanvas::cableAt (Point<float> pos, graph::Edge& hit) const
{
    for (const auto& e : proc.graphEdges())
    {
        const auto a = outPort (e.from);
        const auto path = cablePath (a, inPortFor (e.to, a.y));
        Point<float> nearest;
        path.getNearestPoint (pos, nearest);
        if (nearest.getDistanceFrom (pos) < 6.0f)
        {
            hit = e;
            return true;
        }
    }
    return false;
}

void GraphCanvas::paint (Graphics& g)
{
    g.setColour (ui::kBg.darker (0.2f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    // Cables.
    for (const auto& e : proc.graphEdges())
    {
        const auto a = outPort (e.from);
        g.setColour (ui::kDim.withAlpha (0.55f));
        g.strokePath (cablePath (a, inPortFor (e.to, a.y)),
                      PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    if (cableFrom >= 0)
    {
        g.setColour (ui::kText.withAlpha (0.9f));
        g.strokePath (cablePath (outPort (cableFrom), mousePos),
                      PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // OUT bus bar.
    {
        const auto r = nodeRect (graph::kOut);
        g.setColour (Colour (0xff17181c));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (selected == graph::kOut ? ui::kText : ui::kAccent.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, selected == graph::kOut ? 1.3f : 1.0f);
        g.setColour (ui::kText);
        g.setFont (ui::sans (9.0f, true));
        const char* letters = "OUT";
        for (int i = 0; i < 3; ++i)
            g.drawText (String::charToString ((juce_wchar) letters[i]),
                        Rectangle<float> (r.getX(), r.getCentreY() - 21.0f + (float) i * 14.0f,
                                          r.getWidth(), 14.0f),
                        Justification::centred);
    }

    // Cards.
    for (int id = 0; id < graph::kNumNodes; ++id)
    {
        if (id == graph::kOut || ! nodeVisible (id))
            continue;
        const auto r = nodeRect (id);
        const bool isVoice = graph::kindOf (id) == graph::NodeKind::Voice;
        const auto onId = powerParam (id);
        const bool on = onId.isEmpty()
                        || proc.apvts.getRawParameterValue (onId)->load() > 0.5f;
        // Voices whose mode is Off fade out of the picture.
        const bool voiceActive = ! isVoice
                                 || (int) proc.apvts
                                            .getRawParameterValue ("v" + String (id % 8 + 1) + "Mode")
                                            ->load()
                                        != 0;
        const float alpha = voiceActive ? 1.0f : 0.4f;

        g.setColour (Colour (0xff17181c).withAlpha (alpha));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour ((id == selected ? ui::kText : Colour (0xff2c2f34)).withAlpha (alpha));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, id == selected ? 1.3f : 1.0f);

        float textX = r.getX() + 6.0f;
        if (onId.isNotEmpty())
        {
            const auto dot = Rectangle<float> (6.0f, 6.0f)
                                 .withCentre ({ r.getX() + 9.0f, r.getCentreY() });
            g.setColour (on ? ui::kText : ui::kDim.withAlpha (0.35f));
            if (on)
                g.fillEllipse (dot);
            else
                g.drawEllipse (dot, 1.0f);
            textX = dot.getRight() + 4.0f;
        }

        const auto tint = isVoice ? ui::voiceInk (id - graph::kVoice0)
                                  : (on ? ui::kDim.brighter (0.35f) : ui::kDim.withAlpha (0.45f));
        g.setColour (tint.withAlpha (alpha));
        g.setFont (ui::sans (9.0f, true));
        g.drawText (nodeName (id),
                    Rectangle<float> (textX, r.getY(), r.getRight() - textX - 6.0f, r.getHeight()),
                    Justification::centredLeft);

        g.setColour (ui::kAccent.withAlpha (alpha));
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (outPort (id)));
        if (! isVoice)
            g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (inPort (id)));
    }

    g.setColour (ui::kDim.withAlpha (0.45f));
    g.setFont (ui::sans (8.5f));
    g.drawText ("drag a port to wire - drop a node on a cable to splice it in - right-click to cut / add",
                getLocalBounds().reduced (8, 3), Justification::bottomRight);

    g.setColour (ui::kBorder);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
}

void GraphCanvas::mouseDown (const MouseEvent& e)
{
    mousePos = e.position;

    if (e.mods.isPopupMenu())
    {
        graph::Edge hit;
        if (cableAt (e.position, hit))
        {
            proc.graphRemoveEdge (hit.from, hit.to);
            repaint();
            return;
        }
        showAddMenu (e.getPosition());
        return;
    }

    const int id = nodeAt (e.position);
    if (id < 0)
        return;

    const auto r = nodeRect (id);
    if (id != graph::kOut && e.position.x > r.getRight() - 10.0f)
    {
        cableFrom = id;
        return;
    }
    const auto onId = powerParam (id);
    if (onId.isNotEmpty() && e.position.x < r.getX() + 15.0f)
    {
        if (auto* p = proc.apvts.getParameter (onId))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
    }
    if (id != graph::kOut)
    {
        dragNode = id;
        dragOffset = e.position - r.getPosition();
    }
    selected = id;
    onSelect (id);
    repaint();
}

void GraphCanvas::mouseDrag (const MouseEvent& e)
{
    mousePos = e.position;
    if (dragNode >= 0)
    {
        const auto p = (e.position - dragOffset).toInt();
        proc.setNodePos (dragNode,
                         { jlimit (0, getWidth() - (int) (kCardW + kBusW + 12.0f), p.x),
                           jlimit (0, getHeight() - (int) kCardH, p.y) });
    }
    repaint();
}

void GraphCanvas::mouseUp (const MouseEvent& e)
{
    if (cableFrom >= 0)
    {
        const int target = nodeAt (e.position);
        if (target >= 0 && target != cableFrom
            && graph::kindOf (target) != graph::NodeKind::Voice)
            proc.graphAddEdge (cableFrom, target); // rejects cycles internally
        cableFrom = -1;
    }
    else if (dragNode >= 0)
    {
        // Dropping an unwired node onto a cable splices it into that lane.
        bool unwired = true;
        for (const auto& ed : proc.graphEdges())
            if (ed.from == dragNode || ed.to == dragNode)
            {
                unwired = false;
                break;
            }
        graph::Edge hit;
        if (unwired && graph::kindOf (dragNode) != graph::NodeKind::Voice
            && dragNode != graph::kOut && cableAt (nodeRect (dragNode).getCentre(), hit))
        {
            proc.graphRemoveEdge (hit.from, hit.to);
            if (! proc.graphAddEdge (hit.from, dragNode)
                || ! proc.graphAddEdge (dragNode, hit.to))
            {
                // Shouldn't happen (a splice can't cycle), but never lose the
                // original connection.
                proc.graphAddEdge (hit.from, hit.to);
            }
        }
    }
    dragNode = -1;
    repaint();
}

void GraphCanvas::showAddMenu (Point<int> canvasPos)
{
    PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    PopupMenu eqs, comps, sats, gains, echoes, verbs;
    for (int i = 0; i < 8; ++i)
    {
        if (! proc.nodeOnCanvas (graph::kEq0 + i))
            eqs.addItem (100 + i, "EQ " + String (i + 1));
        if (! proc.nodeOnCanvas (graph::kComp0 + i))
            comps.addItem (200 + i, "COMP " + String (i + 1));
        if (! proc.nodeOnCanvas (graph::kSat0 + i))
            sats.addItem (300 + i, "SAT " + String (i + 1));
    }
    for (int i = 0; i < 4; ++i)
        if (! proc.nodeOnCanvas (graph::kGain0 + i))
            gains.addItem (400 + i, "GAIN " + String (i + 1));
    for (int i = 0; i < 2; ++i)
    {
        if (! proc.nodeOnCanvas (graph::kEcho0 + i))
            echoes.addItem (500 + i, "ECHO " + String (i + 1));
        if (! proc.nodeOnCanvas (graph::kVerb0 + i))
            verbs.addItem (600 + i, "REVERB " + String (i + 1));
    }
    menu.addSubMenu ("Add EQ", eqs);
    menu.addSubMenu ("Add COMP", comps);
    menu.addSubMenu ("Add SAT", sats);
    menu.addSubMenu ("Add GAIN (leveled summer)", gains);
    menu.addSubMenu ("Add ECHO", echoes);
    menu.addSubMenu ("Add REVERB", verbs);
    menu.addSeparator();
    menu.addItem (900, "Wire classic chains (EQ-COMP-SAT per voice)");
    menu.addItem (901, "Reset to default patch");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this),
                        [this, canvasPos] (int result)
                        {
                            if (result <= 0)
                                return;
                            if (result == 900)
                            {
                                proc.graphResetToDefault();
                                for (const auto& ed : graph::chainEdges())
                                    proc.graphAddEdge (ed.from, ed.to);
                                // Drop the default V->OUT wires the chains replace.
                                for (int v = 0; v < 8; ++v)
                                    proc.graphRemoveEdge (graph::kVoice0 + v, graph::kOut);
                                repaint();
                                return;
                            }
                            if (result == 901)
                            {
                                proc.graphResetToDefault();
                                repaint();
                                return;
                            }
                            const int base[] = { graph::kEq0, graph::kComp0,
                                                 graph::kSat0, graph::kGain0,
                                                 graph::kEcho0, graph::kVerb0 };
                            const int id = base[result / 100 - 1] + result % 100;
                            proc.ensureNodeVisible (id, canvasPos);
                            repaint();
                        });
}
