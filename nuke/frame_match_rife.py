"""Build actual Cattery RIFE interpolation branches; no frame analysis here."""
import nuke


def gaps(first, last, mapping, missing):
    result, unresolved = [], []
    frame = first
    while frame <= last:
        if not missing[frame]:
            frame += 1
            continue
        start = frame
        while frame <= last and missing[frame]:
            frame += 1
        end = frame - 1
        left, right = start - 1, end + 1
        if left < first or right > last or mapping[left] >= mapping[right]:
            unresolved.extend(range(start, end + 1))
        else:
            result.append(dict(start=start, end=end, left=left, right=right,
                               result_left=mapping[left], result_right=mapping[right],
                               count=end-start+1))
    return result, unresolved


def export_rife(analyzer, warp, parent=None, raw_input=None, single_input=False):
    first, last = int(analyzer['sourceFirst'].value()), int(analyzer['sourceLast'].value())
    mapping = {f: int(round(analyzer['inputFrame'].valueAt(f))) for f in range(first, last+1)}
    missing = {f: analyzer['missing'].valueAt(f) > .5 for f in mapping}
    segments, unresolved = gaps(first, last, mapping, missing)
    path = analyzer.fullName().rpartition('.')[0]
    parent = parent or (nuke.toNode(path) if path else nuke.root())
    identity = analyzer['fm_export_id'].value()
    group = next((n for n in parent.nodes() if n.knob('fm_rife_id')
                  and n['fm_rife_id'].value() == identity), None)
    # Resolve installed RIFE before modifying any existing generated branch.
    with parent:
        for sibling in parent.nodes():
            sibling['selected'].setValue(False)
        probe = nuke.createNode('FrameMatchRIFE', inpanel=False) if segments else None
        if probe:
            try:
                for name in ('timing', 'timingFrame', 'in', 'out', 'skipKeyframes'):
                    if not probe.knob(name):
                        raise RuntimeError('Installed RIFE lacks '+name)
            finally:
                nuke.delete(probe)
        if group is None:
            from frame_match_complete import unique_name
            group = nuke.nodes.Group(name=unique_name('RIFE_Fill_Gaps',parent))
            key = nuke.String_Knob('fm_rife_id', '');key.setVisible(False);group.addKnob(key);key.setValue(identity)
            for name, label in [('gap_count', 'Gap segments'), ('generated_count', 'Frames to synthesize'),
                                ('unresolved_count', 'Missing without two endpoints')]:
                knob = nuke.Int_Knob(name, label);group.addKnob(knob);knob.setEnabled(False)
            group.addKnob(nuke.Text_Knob('note', '', 'Actual RIFE nodes inside. Regenerated on analysis. No automatic Viewer connection.'))
            if warp is not None:group.setXYpos(warp.xpos()+350, warp.ypos())
        with group:
            for child in list(group.nodes()):
                nuke.delete(child)
            raw = nuke.nodes.Input(name='TimeWarp' if single_input else 'Result', number=0)
            base = raw if single_input else nuke.nodes.Input(name='TimeWarp', number=1)
            raw.setXYpos(0, 0)
            if base is not raw:base.setXYpos(-200, 0)
            output = base
            for index, gap in enumerate(segments):
                x = index*380
                left = nuke.nodes.FrameHold(name='Left_%d'%gap['left'])
                left['first_frame' if left.knob('first_frame') else 'firstFrame'].setValue(gap['left'] if single_input else gap['result_left'])
                right = nuke.nodes.FrameHold(name='Right_%d'%gap['right'])
                right['first_frame' if right.knob('first_frame') else 'firstFrame'].setValue(gap['right'] if single_input else gap['result_right'])
                left.setInput(0, raw);right.setInput(0, raw)
                left.setXYpos(x,100);right.setXYpos(x+140,100)
                pair = nuke.nodes.Switch(name='Endpoint_Pair_%d'%index)
                pair.setInput(0,left);pair.setInput(1,right);pair['which'].setExpression('frame < 2 ? 0 : 1');pair.setXYpos(x,180)
                for child in group.nodes():
                    child['selected'].setValue(False)
                rife = nuke.createNode('FrameMatchRIFE', inpanel=False);rife.setName('RIFE_%d_%d'%(gap['start'],gap['end']))
                rife.setInput(0,pair);rife['in'].setValue(1);rife['out'].setValue(2)
                rife['timing'].setValue('Frame');rife['skipKeyframes'].setValue(True)
                rife['timingFrame'].setExpression('1 + clamp((frame - %d) / %d.0, 0, 1)'%(gap['left'],gap['right']-gap['left']))
                # The local Cattery wrapper is safe even during initial loading.
                # Gap filling uses Frame mode directly.
                rife['outputFrame'].setExpression('timingFrame')
                rife['timingFrame'].setVisible(True);rife['speed'].setVisible(False)
                rife['label'].setValue('Source %d..%d: %d new frames\nResult endpoints %d / %d'%(gap['start'],gap['end'],gap['count'],gap['result_left'],gap['result_right']))
                rife.setXYpos(x,260)
                patch = nuke.nodes.Switch(name='Fill_%d_%d'%(gap['start'],gap['end']))
                patch.setInput(0,output);patch.setInput(1,rife)
                patch['which'].setExpression('frame >= %d && frame <= %d'%(gap['start'],gap['end']))
                patch.setXYpos(x,380);output=patch
            final = nuke.nodes.Output();final.setInput(0,output);final.setXYpos(output.xpos(),output.ypos()+100)
        if single_input:
            group.setInput(0,warp)
            group['note'].setValue('Connect the exported TimeWarp output here. One input supplies both gap endpoints and unchanged frames.')
        else:
            group.setInput(0,raw_input if raw_input is not None else analyzer.input(0));group.setInput(1,warp)
        group['gap_count'].setValue(len(segments));group['generated_count'].setValue(sum(s['count'] for s in segments))
        group['unresolved_count'].setValue(len(unresolved))
        group['label'].setValue('RIFE GAP FILL / EXPORTED\n[value gap_count] gaps / [value generated_count] new frames\nUnresolved: [value unresolved_count]')
    return group, segments, unresolved
