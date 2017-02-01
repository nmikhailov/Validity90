EXTRA_DIST = doxygen.cfg

docs: doxygen.cfg
	doxygen $^

docs-upload: docs
	ln -s html api
	ncftpput -f ~/.ncftp/reactivated -m -R httpdocs/fprint api/
	rm -f api

