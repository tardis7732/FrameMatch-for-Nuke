import nuke


def _matchframe_native_align(node):
    """Use the real MatchGrade button event, including its native C++ action."""
    if not nuke.env.get('gui'):
        raise RuntimeError('MatchGrade native alignment must run in the NukeX GUI.')
    if not nuke.env.get('nukex') and not nuke.env.get('studio'):
        raise RuntimeError('MatchGrade requires NukeX.')
    from PySide6.QtCore import Qt, QEventLoop
    from PySide6.QtWidgets import QApplication, QAbstractButton
    from PySide6.QtTest import QTest

    source, mask = node.input(1), node.input(2)
    original_input = node.input(0)
    if source is None or original_input is None:
        raise RuntimeError('Connect Change and Source before aligning.')
    old_r = old_t = None
    raw = original_input
    if raw.Class() == 'Reformat' and raw.knob('matchframe_generated'):
        candidate = raw.input(0)
        if candidate and candidate.Class() == 'Transform' and candidate.knob('matchframe_generated'):
            old_r, old_t = raw, candidate
            raw = candidate.input(0)
    if raw is None:
        raise RuntimeError('Change input is disconnected.')

    parent_path = node.fullName().rpartition('.')[0]
    parent = nuke.toNode(parent_path) if parent_path else nuke.root()
    frame = int(node['reference_frame'].value())
    previous_frame = nuke.frame()
    temporary = []
    generated = []
    selected = [n for n in parent.nodes() if n['selected'].value()]
    try:
        nuke.frame(frame)
        with parent:
            target = raw
            if mask is not None and node['use_mask'].value():
                merge = nuke.nodes.Merge2(name='__matchframe_mask_analysis')
                merge['operation'].setValue('over')
                merge.setInput(0, raw)
                merge.setInput(1, mask)
                temporary.append(merge)
                target = merge
            native = nuke.nodes.MatchGrade(name='__matchframe_native_analysis')
            temporary.append(native)
            native.setInput(0, source)
            native.setInput(1, target)
            native['sourceRefFrames'].fromScript('{curve x%d %d}' % (frame, frame))
            from frame_match_native_ui import native_button
            with native_button(native, 'Align Target to Source') as native_align:
                QTest.mouseClick(native_align, Qt.LeftButton)
            r = native.input(1)
            if r is target or r.Class() != 'Reformat' or r.input(0).Class() != 'Transform':
                raise RuntimeError('Native MatchGrade did not produce Transform / Reformat.')
            t = r.input(0)
            generated = [r, t]
            # Keep Foundry's computed nodes/settings; remove only the analysis mask.
            t.setInput(0, raw)
            native.hideControlPanel()
            if old_t is not None:
                for key in ('translate', 'rotate', 'scale', 'skewX', 'skewY', 'skew_order',
                            'center', 'invert_matrix', 'filter', 'black_outside', 'motionblur'):
                    if old_t.knob(key) is not None and t.knob(key) is not None:
                        old_t[key].fromScript(t[key].toScript())
                for key in ('type', 'box_width', 'box_height', 'box_fixed', 'resize',
                            'center', 'filter', 'black_outside'):
                    if old_r.knob(key) is not None and r.knob(key) is not None:
                        old_r[key].fromScript(r[key].toScript())
                nuke.delete(r); nuke.delete(t)
                generated = []
                t, r = old_t, old_r
            else:
                t.setName('Transform_Change')
                r.setName('Reformat_Change')
                for created in (t, r):
                    marker = nuke.Boolean_Knob('matchframe_generated', '')
                    marker.setValue(True); marker.setVisible(False); created.addKnob(marker)
            node.setInput(0, r)
            t.setXYpos(raw.xpos(), raw.ypos()+110)
            r.setXYpos(raw.xpos(), raw.ypos()+160)
            if node.ypos() < r.ypos()+70:
                node.setYpos(r.ypos()+70)
            generated = []
        node['status'].setValue('Native MatchGrade aligned at frame %d' % frame)
    except Exception:
        node.setInput(0, original_input)
        for created in generated:
            nuke.delete(created)
        node['status'].setValue('Alignment failed; previous connection retained')
        raise
    finally:
        for created in reversed(temporary):
            nuke.delete(created)
        nuke.frame(previous_frame)
        for sibling in parent.nodes():
            sibling['selected'].setValue(sibling in selected)


