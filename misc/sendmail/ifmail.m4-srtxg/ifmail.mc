divert(-1)
define(`LOCAL_LOGIN_NAME',      ``emartine'')
define(`SMART_HOST',            ``[smtp.your.provider.com]'')
define(`FIDO_HOST_ADDRESS',     ``f15.n2.z3.fidonet.org'')
define(`FIDO_SMART_HOST',       ``f1.n2.z3.fidonet.org'')
define(`FIDO_GATEWAY',          ``f4.n5.z6.fidonet.org'')
define(`ISP_LOGIN_NAME',        ``emartine'')
define(`ISP_MAIL_DOMAIN',       ``provider.com'')
define(`USE_FGATE')

include(`../m4/cf.m4')
define(`confDEF_USER_ID',``8:12'')
define(`confMATCH_GECOS',`True')
define(`confTRY_NULL_MX_LIST',`True')
define(`confTO_QUEUEWARN', `2d')
define(`confTO_QUEUERETURN', `8d')
define(`confTRUSTED_USERS',`fnet')
OSTYPE(`linux')
undefine(`UUCP_RELAY')
undefine(`BITNET_RELAY')
FEATURE(redirect)
FEATURE(always_add_domain)
FEATURE(use_cw_file)
FEATURE(local_procmail)
FEATURE(nodns)
FEATURE(nocanonify)
FEATURE(mailertable)
MAILER(procmail)
MAILER(smtp)
MAILER(fnet)
MAILER(uucp)

LOCAL_CONFIG
# Pseudo-domains (don't call the DNS on them)
CPz1.fidonet.org z2.fidonet.org z3.fidonet.org z4.fidonet.org
CPz5.fidonet.org z6.fidonet.org ftn

# Domains and addresses we refuse mail from
FR/etc/sendmail.rej

# for fidonet address, don't send trough fnet mailer addresses like
# smtp.z2.fidonet.org, www.z2.fidonet.org, etc
CFfidonet ns ns2 mail smtp www ftp

LOCAL_NET_CONFIG
# ************ FIDONET.ORG ***********
# for nodes allways put leading $* if you want to route his points too
# routed trough default smart host FIDO_SMART_HOST
R$* < @ $~F $+ .z1.fidonet.org . > $*   $#fnet $@ FIDO_SMART_HOST $: $1 < @ $2 $3 .z1.fidonet.org > $4
R$* < @ $~F $+ .z2.fidonet.org . > $*   $#fnet $@ FIDO_SMART_HOST $: $1 < @ $2 $3 .z2.fidonet.org > $4
R$* < @ $~F $+ .z3.fidonet.org . > $*   $#fnet $@ FIDO_SMART_HOST $: $1 < @ $2 $3 .z3.fidonet.org > $4
R$* < @ $~F $+ .z4.fidonet.org . > $*   $#fnet $@ FIDO_SMART_HOST $: $1 < @ $2 $3 .z4.fidonet.org > $4
R$* < @ $~F $+ .z5.fidonet.org . > $*   $#fnet $@ FIDO_SMART_HOST $: $1 < @ $2 $3 .z5.fidonet.org > $4
R$* < @ $~F $+ .z6.fidonet.org . > $*   $#fnet $@ FIDO_SMART_HOST $: $1 < @ $2 $3 .z6.fidonet.org > $4

# all the remaining FTN's (via fido smart host FIDO_SMART_HOST)
R$* < @ $+ .ftn . > $*                  $#fnet $@ FIDO_SMART_HOST $: $1 < @ $2 .ftn > $3

# If you don't have internet connectivity comment out this line
# to send through the gateway FIDO_GATEWAY
# (packets will go to your default uplink FIDO_SMART_HOST)
ifdef(`USE_FGATE',`',`#')R$* < @ $* > $*                        $#fnet $@ FIDO_SMART_HOST $: $1 % $2 < @ FIDO_GATEWAY > $3

LOCAL_RULE_0
#  ************ ME (and local) ***************
ifdef(`ISP_LOGIN_NAME', `', `#')R ifdef(`ISP_LOGIN_NAME',`ISP_LOGIN_NAME',`login')<@ ifdef(`ISP_MAIL_DOMAIN',`ISP_MAIL_DOMAIN',`provider.com') > $*             $#local $: LOCAL_LOGIN_NAME

###################
# ANTI-SPAM RULES #
###################

Scheck_mail
R < $=R >                       $#error $@ 5.7.1 $: "571 unsolicited email is refused"
R $=R                           $#error $@ 5.7.1 $: "571 unsolicited email is refused"
R$*                             $: $>3 $1
R $+ < @ $* $=R >               $#error $@ 5.7.1 $: "571 unsolicited email is refused"
R $+ < @ $* $=R . >             $#error $@ 5.7.1 $: "571 unsolicited email is refused"
R$*                             $@ OK

Scheck_compat
#sender-address $| recipient-address
R $+ % $* @ FIDO_GATEWAY $| $*  $: $1 @ $2 $| $3
R < $+ @ $* $=R > $| $*         $#error $@ 5.7.1 $: "571 unsolicited email is refused"
R $+ @ $* $=R $| $*             $#error $@ 5.7.1 $: "571 unsolicited email is refused"
R < $=R > $| $*                 $#error $@ 5.7.1 $: "571 unsolicited email is refused"
R $=R $| $*                     $#error $@ 5.7.1 $: "571 unsolicited email is refused"

# To: "UUCP" at local is returned back. Because it happens whith ifmail
# receiving mail to "UUCP" via FTN but whitout a To: line specifying a
# destination ! 8-)
R $* fidonet.org $| UUCP        $#error $: "You MUST provide a To: address"
R $* ftn $| UUCP                $#error $: "You MUST provide a To: address"

R$*                             $@ OK

