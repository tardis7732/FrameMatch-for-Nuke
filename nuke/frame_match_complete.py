"""Analysis data container with explicit native-node exports."""
import json
import nuke


def button(tool,name,label,code):
    k=nuke.PyScript_Knob(name,label);tool.addKnob(k)
    k.setValue('import frame_match_complete as fm\n'+code)
    k.setFlag(nuke.STARTLINE)
    return k


def create(result=None,source=None,mask=None):
    import frame_match_ofx_bridge as bridge
    bridge.register()
    tool=nuke.nodes.Group(name=unique_name('FrameMatch',nuke.thisGroup()))
    with tool:
        raw=nuke.nodes.Input(name='Change',number=0)
        ref=nuke.nodes.Input(name='Source',number=1)
        mask_input=nuke.nodes.Input(name='Mask',number=2)
        a=nuke.createNode(bridge.CLASS,inpanel=False);a.setName('Analyzer');a['analyze'].setLabel('Analyze frame timing');a.setInput(0,raw);a.setInput(1,ref)
        out=nuke.nodes.Output(name='Output1');out.setInput(0,raw)
    tool.addKnob(nuke.Tab_Knob('frame_match','Frame Match'))
    for name in ('fm_complete_tool','spatial_ready'):
        k=nuke.Boolean_Knob(name,'');tool.addKnob(k);k.setVisible(False)
    tool['fm_complete_tool'].setValue(True)
    k=nuke.String_Knob('spatial_data','');tool.addKnob(k);k.setVisible(False)
    k=nuke.Int_Knob('reference_frame','Reference frame');tool.addKnob(k);k.setValue(nuke.frame())
    button(tool,'use_current','Current',"nuke.thisNode()['reference_frame'].setValue(nuke.frame())").clearFlag(nuke.STARTLINE)
    k=nuke.Boolean_Knob('use_mask','Use mask');tool.addKnob(k);k.setValue(True)
    k.setTooltip('Use Mask for MatchGrade alignment only.')
    button(tool,'align','Analyze align','fm.align(nuke.thisNode())')
    button(tool,'analyze','Analyze timing','fm.analyze_timing(nuke.thisNode())').clearFlag(nuke.STARTLINE)
    for prefix,label,first,last in [('source','Source range','sourceFirst','sourceLast'),('result','Change range','resultFirst','resultLast')]:
        k=nuke.Int_Knob(prefix+'_first',label);tool.addKnob(k);k.setValue(int(a[first].value()))
        k=nuke.Int_Knob(prefix+'_last','');tool.addKnob(k);k.setValue(int(a[last].value()));k.clearFlag(nuke.STARTLINE)
        button(tool,'reset_'+prefix,'Reset',"fm.reset_range(nuke.thisNode(), %r)"%prefix).clearFlag(nuke.STARTLINE)
    for name,label in [('mapped_frame','Matched change frame'),('matched_count','Matched source frames'),('missing_count','Missing source frames'),('review_count','Review frames'),('fill_count','RIFE frames'),('unresolved_count','Without endpoints')]:
        k=nuke.Int_Knob(name,label);tool.addKnob(k);k.setVisible(False)
    tool.addKnob(nuke.Text_Knob('state','Timing','Not analyzed'))
    tool.addKnob(nuke.Text_Knob('spatial_status','Alignment','Not analyzed'))
    tool['state'].setVisible(False);tool['spatial_status'].setVisible(False)
    tool.addKnob(nuke.Text_Knob('exports','Export',' '))
    for name,label,kind in [('export_spatial','Align','spatial'),('export_timewarp','TimeWarp','timewarp'),('export_rife','RIFE','rife')]:
        button(tool,name,label,"fm.export(nuke.thisNode(), %r)"%kind).clearFlag(nuke.STARTLINE)
    button(tool,'export_all','Export all',"fm.export(nuke.thisNode(), 'all')").clearFlag(nuke.STARTLINE)
    tool.addKnob(nuke.Text_Knob('results','', '<b>Results</b><br>Not analyzed.'))
    tool.addKnob(nuke.Tab_Knob('settings','Settings'))
    k=nuke.Boolean_Knob('rife_gpu','RIFE GPU');tool.addKnob(k);k.setValue(True)
    k=nuke.Int_Knob('rife_detail','RIFE detail');tool.addKnob(k);k.setValue(3);k.setRange(0,4)
    k=nuke.Enumeration_Knob('rife_downrez','RIFE resolution',['Full','1/2','1/4']);tool.addKnob(k)
    tool.addKnob(nuke.Text_Knob('help','','Change = changed video; Source = original. Mask is used for alignment only.\nTiming uses saved alignment if available; otherwise Change directly. No timing Mask. Analysis stores data only. Output passes Change through unchanged.\nAnalyze automatically creates or updates connected outputs below Change. Manual Export makes independent copies.\nReanalyze after changing inputs. Counts are matching candidates.'))
    raw.setXYpos(0,0);ref.setXYpos(300,0);mask_input.setXYpos(600,0);a.setXYpos(150,120);out.setXYpos(0,260)
    tool['label'].setValue('Source [frame] -> Change [value mapped_frame]')
    tool['knobChanged'].setValue('import frame_match_complete as fm\nfm.changed(nuke.thisNode(),nuke.thisKnob())')
    if result is not None:tool.setInput(0,result)
    if source is not None:tool.setInput(1,source)
    if mask is not None:tool.setInput(2,mask)
    return tool


def changed(tool,k):
    names={'source_first':'sourceFirst','source_last':'sourceLast','result_first':'resultFirst','result_last':'resultLast'}
    if k.name() in names:
        a=tool.node('Analyzer')[names[k.name()]]
        if int(a.value())!=int(k.value()):
            a.setValue(int(k.value()))
            tool.node('Analyzer')['ready'].setValue(False)
            update_results(tool)


def use_ranges(tool):
    raw,ref=tool.input(0),tool.input(1)
    if raw is None or ref is None:raise RuntimeError('Connect Change and Source.')
    for k,v in [('source_first',ref.firstFrame()),('source_last',ref.lastFrame()),('result_first',raw.firstFrame()),('result_last',raw.lastFrame())]:tool[k].setValue(int(v))


def synchronize(tool):
    from frame_match_rife import gaps
    a=tool.node('Analyzer');first,last=int(a['sourceFirst'].value()),int(a['sourceLast'].value())
    mapping={f:int(round(a['inputFrame'].valueAt(f))) for f in range(first,last+1)}
    segments,unresolved=gaps(first,last,mapping,{f:a['missing'].valueAt(f)>.5 for f in mapping})
    k=tool['mapped_frame'];k.clearAnimated();k.setAnimated()
    for f,v in mapping.items():k.setValueAt(v,f)
    k.animation(0).changeInterpolation(k.animation(0).keys(),nuke.CONSTANT)
    for name,target in [('matched_count','matchedCount'),('missing_count','missingCount'),('review_count','reviewCount')]:tool[name].setValue(int(a[target].value()))
    tool['fill_count'].setValue(sum(s['count'] for s in segments));tool['unresolved_count'].setValue(len(unresolved))
    tool['state'].setValue('Analysis stored. Export when ready.')
    update_results(tool)


def align(tool):
    from frame_match_spatial import _matchframe_native_align
    if tool.input(0) is None or tool.input(1) is None:raise RuntimeError('Connect Change and Source.')
    parent=parent_of(tool);before=set(parent.nodes());selected=[n for n in parent.nodes() if n['selected'].value()]
    connections={n:[n.input(i) for i in range(n.inputs())] for n in before}
    try:
        with parent:
            proxy=nuke.nodes.Group(name='__SpatialAnalysis');proxy.setInput(0,tool.input(0));proxy.setInput(1,tool.input(1));proxy.setInput(2,tool.input(2))
            for k in (nuke.Int_Knob('reference_frame',''),nuke.Boolean_Knob('use_mask',''),nuke.Text_Knob('status','')):proxy.addKnob(k)
            proxy['use_mask'].setValue(bool(tool['use_mask'].value()))
            proxy['reference_frame'].setValue(int(tool['reference_frame'].value()))
            _matchframe_native_align(proxy)
            r=proxy.input(0);t=r.input(0)
            data={}
            for node in (t,r):
                data[node.Class()]={name:k.toScript() for name,k in node.knobs().items() if name not in ('name','xpos','ypos','selected','label','knobChanged','onCreate','onDestroy') and k.Class() not in ('PyScript_Knob','Tab_Knob')}
            tool['spatial_data'].setValue(json.dumps(data));tool['spatial_ready'].setValue(True)
            tool.node('Analyzer')['ready'].setValue(False)
            tool['state'].setValue('Alignment updated. Analyze timing again.')
            tool['spatial_status'].setValue('Native MatchGrade data stored at frame %d'%tool['reference_frame'].value())
    finally:
        for n in list(parent.nodes()):
            if n not in before:nuke.delete(n)
        # Native MatchGrade can insert nodes into other dependent branches.
        # Restore every original connection after deleting temporary nodes.
        for n,inputs in connections.items():
            for i in range(max(n.inputs(),len(inputs))):n.setInput(i,inputs[i] if i<len(inputs) else None)
        for n in parent.nodes():n['selected'].setValue(n in selected)
        update_results(tool)
    auto_export(tool,'spatial')


def parent_of(tool):
    path=tool.fullName().rpartition('.')[0]
    return nuke.toNode(path) if path else nuke.root()


def unique_name(base,parent):
    names={n.name() for n in parent.nodes()};name=base;index=1
    while name in names:name=base+str(index);index+=1
    return name


def make_warp(a,raw,name='TimeWarp_FrameMatch'):
    w=nuke.nodes.TimeWarp(name=name);w.setInput(0,raw);w['filter'].setValue('nearest')
    w['lookup'].fromScript(a['inputFrame'].toScript())
    curve=w['lookup'].animation(0)
    if curve:curve.changeInterpolation(curve.keys(),nuke.CONSTANT)
    w['label'].setValue('Source [frame] -> Change [value lookup]')
    return w


def export(tool,kind):
    a=tool.node('Analyzer');parent=parent_of(tool);raw=None
    if kind=='spatial' and not has_alignment(tool):raise RuntimeError('Analyze alignment first.')
    if kind!='spatial' and not a['ready'].value():raise RuntimeError('Analyze frame timing first.')
    before=set(parent.nodes());selected=[n for n in parent.nodes() if n['selected'].value()]
    undo=nuke.Undo();undo.begin('Export FrameMatch '+kind)
    try:
        with parent:
            for n in parent.nodes():n['selected'].setValue(False)
            if kind=='spatial' or (kind=='all' and has_alignment(tool)):
                data=json.loads(tool['spatial_data'].value())
                for cls in ('Transform','Reformat'):
                    node=getattr(nuke.nodes,cls)(name=unique_name(cls+'_FrameMatch',parent))
                    for name,value in data[cls].items():
                        if node.knob(name):node[name].fromScript(value)
                    node.setInput(0,raw);raw=node
            w=make_warp(a,raw,unique_name('TimeWarp_FrameMatch',parent)) if kind in ('timewarp','all','timing') else None
            if kind in ('rife','all','timing'):
                from frame_match_rife import export_rife
                # Each export is independent, never replace earlier outputs.
                import uuid
                identity=a['fm_export_id'].value();a['fm_export_id'].setValue(uuid.uuid4().hex)
                try:g,_,_=export_rife(a,w,parent=parent,single_input=True)
                finally:a['fm_export_id'].setValue(identity)
                for n in g.nodes():
                    if n.Class() in ('RIFE','FrameMatchRIFE'):
                        n['useGPU'].setValue(tool['rife_gpu'].value());n['detail'].setValue(tool['rife_detail'].value());n['downrez'].setValue(int(tool['rife_downrez'].getValue()))
            created=[n for n in parent.nodes() if n not in before]
            order={'Transform':0,'Reformat':1,'TimeWarp':2,'Group':3}
            created.sort(key=lambda n:order.get(n.Class(),4))
            for i,n in enumerate(created):n.setXYpos(tool.xpos()+300,tool.ypos()+i*100)
            return created
    except Exception:
        for n in list(parent.nodes()):
            if n not in before:nuke.delete(n)
        raise
    finally:
        for n in parent.nodes():n['selected'].setValue(n in selected)
        undo.end()


_DATA = ('inputFrame','confidence','missing','matchedCount','missingCount','reviewCount','status','analysisRevision','ready')


def spatial_chain(tool, raw):
    data=json.loads(tool['spatial_data'].value())
    for cls in ('Transform','Reformat'):
        node=getattr(nuke.nodes,cls)(name='Analysis_'+cls)
        for name,value in data[cls].items():
            if node.knob(name):node[name].fromScript(value)
        node.setInput(0,raw);raw=node
    return raw


def analyze_timing(tool, preserve_timing=False):
    """Run actual OFX on temporary aligned images, retain only analysis data."""
    if not nuke.env.get('gui'):raise RuntimeError('Run Analyze in NukeX GUI.')
    use_alignment=has_alignment(tool)
    if tool.input(0) is None or tool.input(1) is None:raise RuntimeError('Connect Change and Source.')
    from PySide6.QtCore import Qt,QEventLoop
    from PySide6.QtWidgets import QApplication,QAbstractButton
    from PySide6.QtTest import QTest
    stored=tool.node('Analyzer');before=set(tool.nodes());snapshot=None;a=None
    if preserve_timing and not stored['ready'].value():raise RuntimeError('Stored timing is required to repair image holds.')
    selected=[n for n in tool.nodes() if n['selected'].value()]
    revision=int(stored['analysisRevision'].value())
    try:
        with tool:
            import frame_match_ofx_bridge as bridge
            for node in tool.nodes():node['selected'].setValue(False)
            a=nuke.createNode(bridge.CLASS,inpanel=False);a.setName('Analysis_Run')
            if a in bridge._observed:bridge._observed.remove(a)
            for name in ('sourceFirst','sourceLast','resultFirst','resultLast','timingPreference','analysisRevision'):
                a[name].fromScript(stored[name].toScript())
            if preserve_timing:
                for name in _DATA:a[name].fromScript(stored[name].toScript())
                a['repairStoredTiming'].setValue(True)
            aligned=spatial_chain(tool,tool.node('Change')) if use_alignment else tool.node('Change')
            a.setInput(0,aligned);a.setInput(1,tool.node('Source'));a.setInput(2,None)
        a['ready'].setValue(False)
        from frame_match_native_ui import native_button
        with native_button(a, 'Analyze frame timing') as native_analyze:
            QTest.mouseClick(native_analyze,Qt.LeftButton)
            QTest.qWait(300)
        if int(a['analysisRevision'].value())<=revision or not a['ready'].value():
            raise RuntimeError('Timing analysis did not complete: revision %s -> %s, ready=%s, status=%s' % (revision,a['analysisRevision'].value(),a['ready'].value(),a['status'].value()))
        snapshot={name:a[name].toScript() for name in _DATA}
    finally:
        if a is not None:a.hideControlPanel()
        for node in list(tool.nodes()):
            if node not in before:nuke.delete(node)
        for node in tool.nodes():node['selected'].setValue(node in selected)
        if snapshot:
            for name,value in snapshot.items():stored[name].fromScript(value)
            stored['fm_export_revision'].setValue(int(stored['analysisRevision'].value()))
            synchronize(tool)
            tool['state'].setValue('Stored: aligned Change.' if use_alignment else 'Stored: Change without alignment.')
        else:
            stored['ready'].setValue(False)
            tool['state'].setValue('Timing analysis failed or cancelled. Reanalyze before exporting.')
    auto_export(tool,'timing')


def reset_range(tool,prefix):
    node=tool.input(1 if prefix=='source' else 0)
    if node is None:raise RuntimeError('Connect the corresponding input first.')
    tool[prefix+'_first'].setValue(int(node.firstFrame()))
    tool[prefix+'_last'].setValue(int(node.lastFrame()))


def update_results(tool):
    if not tool.knob('results'):return
    a=tool.node('Analyzer')
    alignment=('Frame %d'%tool['reference_frame'].value()) if has_alignment(tool) else ('None' if a['ready'].value() else 'Not analyzed')
    if not a['ready'].value():
        text='<b>Results</b><br>Align: %s<br>Timing: analysis required'%alignment
    else:
        total=int(a['sourceLast'].value()-a['sourceFirst'].value()+1)
        text=('<b>Results</b><br>Align: %s &nbsp; | &nbsp; Source %d - %d<br>'
              '<b>Matched %d / %d</b> &nbsp; | &nbsp; Missing %d<br>'
              'RIFE: %d frames &nbsp; | &nbsp; Without endpoints: %d<br>'
              'Review: %d change frames')%(alignment,int(a['sourceFirst'].value()),int(a['sourceLast'].value()),
              tool['matched_count'].value(),total,tool['missing_count'].value(),tool['fill_count'].value(),
              tool['unresolved_count'].value(),tool['review_count'].value())
    tool['results'].setValue('<table cellpadding="7" bgcolor="#303030"><tr><td>'+text+'</td></tr></table>')


def auto_export(tool,kind):
    """Publish successful analysis below Change; replace only this tool's auto outputs."""
    import uuid
    parent=parent_of(tool);raw=tool.input(0)
    if raw is None:raise RuntimeError('Connect Change before creating outputs.')
    if not tool.knob('fm_auto_id'):
        knob=nuke.String_Knob('fm_auto_id','');knob.setVisible(False);tool.addKnob(knob)
    owner=tool['fm_auto_id'].value()
    if not owner or any(n!=tool and n.knob('fm_auto_id') and n['fm_auto_id'].value()==owner for n in parent.nodes()):
        owner=uuid.uuid4().hex;tool['fm_auto_id'].setValue(owner)
    owned={n['fm_auto_role'].value():n for n in parent.nodes() if n.knob('fm_auto_owner')
           and n['fm_auto_owner'].value()==owner and n.knob('fm_auto_role')}
    roles=('Transform','Reformat') if kind=='spatial' else ('TimeWarp','RIFE')
    old={role:owned[role] for role in roles if role in owned}
    anchor=owned.get('Reformat',raw) if kind=='timing' and has_alignment(tool) else raw
    if raw in old.values():raise RuntimeError('Change must be upstream of the automatic outputs.')
    names={role:node.name() for role,node in old.items()}
    undo=nuke.Undo();undo.begin('Update FrameMatch '+kind+' outputs')
    try:
        created=export(tool,kind)
        new={('RIFE' if n.Class()=='Group' else n.Class()):n for n in created}
        new[roles[0]].setInput(0,anchor)
        for index,role in enumerate(roles):
            node=new[role]
            for name,value in [('fm_auto_owner',owner),('fm_auto_role',role)]:
                knob=nuke.String_Knob(name,'');knob.setVisible(False);node.addKnob(knob);knob.setValue(value)
            previous=old.get(role)
            node.setXYpos(previous.xpos() if previous else anchor.xpos(),
                          previous.ypos() if previous else anchor.ypos()+80*(index+1))
        replacements={previous:new[role] for role,previous in old.items()}
        # Preserve existing consumers of earlier automatic outputs, including a
        # Viewer the user connected themselves. Do not attach other consumers.
        for node in parent.nodes():
            if node in old.values() or node in created:continue
            for i in range(node.inputs()):
                current=node.input(i)
                if current in replacements:node.setInput(i,replacements[current])
        for node in old.values():nuke.delete(node)
        for role,name in names.items():new[role].setName(name)
        return [new[role] for role in roles]
    finally:
        undo.end()


def has_alignment(tool):
    return bool(tool['spatial_ready'].value() and tool['spatial_data'].value().strip())
