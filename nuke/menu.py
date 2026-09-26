import nuke
nuke.menu('Nodes').addCommand('Time/FrameMatch', 'import frame_match_complete; frame_match_complete.create()')
