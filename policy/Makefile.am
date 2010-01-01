polkit_policydir = $(datadir)/polkit-1/actions
dist_polkit_policy_DATA =					\
	org.gnome.color.policy

# You will need a recent intltool or the patch from this bug
# http://bugzilla.gnome.org/show_bug.cgi?id=462312
@INTLTOOL_POLICY_RULE@

EXTRA_DIST = org.gnome.color.policy.in
DISTCLEANFILES = org.gnome.color.policy

clean-local :
	rm -f *~

