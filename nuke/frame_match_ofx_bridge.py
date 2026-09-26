"""Nuke graph integration only. All image analysis stays in the native OFX."""
import nuke
import uuid

CLASS = 'OFXcom.matchframe.FrameMatch_v1'
_pending = set()
_registered = False
_observed = []
_timer = None
_failed_revision = {}


def _parent(node):
    path = node.fullName().rpartition('.')[0]
    return nuke.toNode(path) if path else nuke.root()


def install_node(node=None):
    node = node or nuke.thisNode()
    if node.Class() != CLASS or node.knob('analysisRevision') is None:
        return
    node['analyze'].setLabel('Analyze frame timing')
    if not node.knob('fm_export_id'):
        key = nuke.String_Knob('fm_export_id', '')
        key.setVisible(False); node.addKnob(key)
    identity = node['fm_export_id'].value()
    duplicate = any(n != node and n.Class() == CLASS and n.knob('fm_export_id')
                    and n['fm_export_id'].value() == identity for n in _parent(node).nodes())
    if not identity or duplicate:
        node['fm_export_id'].setValue(uuid.uuid4().hex)
    if node not in _observed:
        _observed.append(node)
    _start_observer()
    if node.knob('fm_export_revision') is None:
        knob = nuke.Int_Knob('fm_export_revision', '')
        knob.setVisible(False)
        node.addKnob(knob)
        knob = nuke.Text_Knob('fm_output_status', 'TimeWarp output', 'Analyze frames to create the output node.')
        node.addKnob(knob)
    node['label'].setValue('ANALYSIS / EXPORT ONLY\n'
                          'Match: Source [frame] -> Result [format %.0f [value inputFrame]]\n'
                          'Matched [value matchedCount] / [expr {[value sourceLast]-[value sourceFirst]+1}]'
                          ' | Missing [value missingCount]')


def export_timewarp(node):
    """Bake a successful native mapping into a real, visible Nuke TimeWarp."""
    install_node(node)
    if not node['ready'].value():
        raise RuntimeError('Analyze frames successfully before exporting TimeWarp.')
    first, last = int(node['sourceFirst'].value()), int(node['sourceLast'].value())
    frames = [int(round(node['inputFrame'].valueAt(f))) for f in range(first, last + 1)]
    if not frames or any(b < a for a, b in zip(frames, frames[1:])):
        raise RuntimeError('Invalid or reverse frame mapping; previous output retained.')
    parent = _parent(node)
    raw = node.input(0)
    if raw is None:
        raise RuntimeError('Connect the Result input before exporting.')
    identity = node['fm_export_id'].value()
    warp = next((n for n in parent.nodes() if n.Class() == 'TimeWarp'
                 and n.knob('fm_generated') and
                 ((n.knob('fm_export_id') and n['fm_export_id'].value() == identity)
                  or (not n.knob('fm_export_id') and n.input(0) == node))), None)
    selected = [n for n in parent.nodes() if n['selected'].value()]
    undo = nuke.Undo()
    undo.begin('FrameMatch TimeWarp output')
    try:
        with parent:
            if warp is None:
                warp = nuke.nodes.TimeWarp(name='TimeWarp_FrameMatch')
                tag = nuke.Boolean_Knob('fm_generated', '')
                tag.setVisible(False); tag.setValue(True); warp.addKnob(tag)
                warp.addKnob(nuke.Tab_Knob('fm_info', 'Frame Match'))
                k = nuke.Double_Knob('fm_source_frame', 'Original frame')
                warp.addKnob(k); k.setExpression('frame'); k.setEnabled(False)
                for name, label in [('fm_matched_count', 'Matched source frames'),
                                    ('fm_total_count', 'Total source frames'),
                                    ('fm_missing_count', 'Missing source frames'),
                                    ('fm_review_count', 'Review result frames')]:
                    k = nuke.Int_Knob(name, label); warp.addKnob(k); k.setEnabled(False)
                warp.setXYpos(node.xpos() + 260, node.ypos() + 110)
            if not warp.knob('fm_export_id'):
                tag = nuke.String_Knob('fm_export_id', '')
                tag.setVisible(False); warp.addKnob(tag)
            warp['fm_export_id'].setValue(identity)
            warp.setInput(0, raw)
            warp['filter'].setValue('nearest')
            # Exported node is self-contained, including after analyzer deletion.
            warp['disable'].clearAnimated(); warp['disable'].setValue(False)
            lookup = warp['lookup']; lookup.clearAnimated(); lookup.setAnimated()
            for f, matched in zip(range(first, last + 1), frames):
                lookup.setValueAt(matched, f)
            curve = lookup.animation(0)
            curve.changeInterpolation(curve.keys(), nuke.CONSTANT)
            for knob, value in [('fm_matched_count', int(node['matchedCount'].value())),
                                ('fm_total_count', len(frames)),
                                ('fm_missing_count', int(node['missingCount'].value())),
                                ('fm_review_count', int(node['reviewCount'].value()))]:
                warp[knob].setValue(value)
            warp['label'].setValue('Source [frame] -> Result [format %.0f [value lookup]]\n'
                                  'Matched [value fm_matched_count] / [value fm_total_count]'
                                  ' | Missing [value fm_missing_count]')
            # Never rewire a Viewer or consumer: export is not automatic apply.
            from frame_match_rife import export_rife
            rife_group, segments, unresolved = export_rife(node, warp)
            node['fm_export_revision'].setValue(int(node['analysisRevision'].value()))
            node['fm_output_status'].setValue(warp.name() + ' | %d / %d source frames matched'
                                              % (int(node['matchedCount'].value()), len(frames))
                                              + ' | RIFE: %d frames / %d unresolved'
                                              % (sum(s['count'] for s in segments), len(unresolved)))
    finally:
        for sibling in parent.nodes():
            sibling['selected'].setValue(sibling in selected)
        undo.end()
    if parent.knob('fm_complete_tool'):
        import frame_match_complete
        frame_match_complete.synchronize(parent)
    return warp


def _complete(node, token):
    try:
        if int(node['analysisRevision'].value()) <= int(node['fm_export_revision'].value()):
            return
        parent = _parent(node)
        if parent.knob("fm_complete_tool"):
            import frame_match_complete
            frame_match_complete.synchronize(parent)
            node["fm_export_revision"].setValue(int(node["analysisRevision"].value()))
        else:
            export_timewarp(node)
    except Exception as error:
        try:
            _failed_revision[token] = int(node['analysisRevision'].value())
        except Exception:
            pass
        try:
            node['fm_output_status'].setValue('TimeWarp export failed: ' + str(error))
            parent=_parent(node)
            if parent.knob('fm_complete_tool'):
                parent['state'].setValue('Output rebuild failed: '+str(error))
        except Exception:
            pass
        nuke.tprint('FrameMatch TimeWarp export failed: ' + str(error))
    finally:
        _pending.discard(token)


def _queue(node):
    if int(node['analysisRevision'].value()) <= int(node['fm_export_revision'].value()):
        return
    token = node.fullName()
    if _failed_revision.get(token) == int(node['analysisRevision'].value()):
        return
    if token in _pending:
        return
    _pending.add(token)
    if nuke.env.get('gui'):
        from PySide6.QtCore import QTimer
        QTimer.singleShot(0, lambda: _complete(node, token))
    else:
        _complete(node, token)


def _observe():
    # Nuke does not emit Python knobChanged for values committed by an OFX
    # parameter suite. Watch only the native success counter, never image data.
    for node in list(_observed):
        try:
            node.fullName()
            parent=_parent(node)
            if parent.knob('fm_complete_tool') and not node['ready'].value() and parent.node('TimeWarp_FrameMatch'):
                text='Needs analysis (cached output retained).'
                if parent['state'].value()!=text:parent['state'].setValue(text)
            if node.knob('fm_export_revision'):
                _queue(node)
        except (ValueError, RuntimeError):
            _observed.remove(node)


def _start_observer():
    global _timer
    if nuke.env.get('gui') and _timer is None:
        from PySide6.QtCore import QTimer
        _timer = QTimer()
        _timer.timeout.connect(_observe)
        _timer.start(200)


def register():
    global _registered
    if _registered:
        return
    _registered = True
    nuke.addOnCreate(install_node, nodeClass=CLASS)

