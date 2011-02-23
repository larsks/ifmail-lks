PUSHDIVERT(-1)
ifdef(`FIDO_MAILER_FLAGS',, `define(`FIDO_MAILER_FLAGS', `8mDFMuSC')')
ifdef(`FIDO_MAILER_PATH',, `define(`FIDO_MAILER_PATH', /usr/lib/ifmail/ifmail)')
ifdef(`FIDO_MAILER_USER',, `define(`FIDO_MAILER_USER', `fnet:uucp')')
ifdef(`FIDO_MAILER_ARGS_H',, `define(`FIDO_MAILER_ARGS_H', `ifmail -r $h -g h $u')')
ifdef(`FIDO_MAILER_ARGS_C',, `define(`FIDO_MAILER_ARGS_C', `ifmail -r $h -g c $u')')
ifdef(`FIDO_MAILER_ARGS_B',, `define(`FIDO_MAILER_ARGS_B', `ifmail -b -r $h -g h $u')')
ifdef(`FIDO_MAILER_ARGS_P',, `define(`FIDO_MAILER_ARGS_P', `ifmail -l-3 -r $h -g h $u')')
POPDIVERT

#############################
# FIDO Mailer specification #
#############################

VERSIONID(`@(#)fnet.m4  1.01 (srtxg@f2219.n293.z2.fidonet.org) 7/7/97')

# normal fnet mailer, pkt as hold
Mfnet,  P=FIDO_MAILER_PATH, F=FIDO_MAILER_FLAGS, S=11, R=21,
        _OPTINS(`FIDO_MAILER_CHARSET', `C=', `, ')U=FIDO_MAILER_USER,
        ifdef(`FIDO_MAILER_MAX', `M=FIDO_MAILER_MAX, ')A=FIDO_MAILER_ARGS_H

# This is for crash mail just in case you need it * USE WHITH CARE *
Mfnet-crash,  P=FIDO_MAILER_PATH, F=FIDO_MAILER_FLAGS, S=11, R=21,
        _OPTINS(`FIDO_MAILER_CHARSET', `C=', `, ')U=FIDO_MAILER_USER,
        ifdef(`FIDO_MAILER_MAX', `M=FIDO_MAILER_MAX, ')A=FIDO_MAILER_ARGS_C

# This doesn't split messages when writting in pkt, of course the node
# receiving the pkt must be able to handle arbitrary size messages.
# if the other end uses ifmail too use this.
Mfnet-big,  P=FIDO_MAILER_PATH, F=FIDO_MAILER_FLAGS, S=11, R=21,
        _OPTINS(`FIDO_MAILER_CHARSET', `C=', `, ')U=FIDO_MAILER_USER,
        ifdef(`FIDO_MAILER_MAX', `M=FIDO_MAILER_MAX, ')A=FIDO_MAILER_ARGS_B

# This one uses a "kludge verbosity" of level -3, that is nothing is kept
# from usenet/email infos.
Mfnet-poor, P=FIDO_MAILER_PATH, F=FIDO_MAILER_FLAGS, S=11, R=21,
        _OPTINS(`FIDO_MAILER_CHARSET', `C=', `, ')U=FIDO_MAILER_USER,
        ifdef(`FIDO_MAILER_MAX', `M=FIDO_MAILER_MAX, ')A=FIDO_MAILER_ARGS_P

