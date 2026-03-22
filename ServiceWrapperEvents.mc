MessageIdTypedef=DWORD

SeverityNames=(
    Success=0x0:STATUS_SEVERITY_SUCCESS
    Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
    Warning=0x2:STATUS_SEVERITY_WARNING
    Error=0x3:STATUS_SEVERITY_ERROR
)

FacilityNames=(
    System=0x0:FACILITY_SYSTEM
)

LanguageNames=(
    English=0x409:MSG00409
)

MessageId=0x1000
Severity=Error
Facility=System
SymbolicName=MSG_START_SETUP_FAILED
Language=English
Service %1 (%2) startup failed with error %3 (%4).
.

MessageId=0x1001
Severity=Error
Facility=System
SymbolicName=MSG_START_FAILED
Language=English
Service %1 (%2) could not start the child process because of error %3 (%4).
.

MessageId=0x1002
Severity=Informational
Facility=System
SymbolicName=MSG_STARTED
Language=English
Service %1 (%2) started child process %3.
.

MessageId=0x1003
Severity=Informational
Facility=System
SymbolicName=MSG_STOP_SKIPPED
Language=English
Service %1 (%2) skipped the stop request because the child process was not running.
.

MessageId=0x1004
Severity=Error
Facility=System
SymbolicName=MSG_TERMINATE_FAILED
Language=English
Service %1 (%2) failed to terminate child process %3 because of error %4 (%5).
.

MessageId=0x1005
Severity=Warning
Facility=System
SymbolicName=MSG_TERMINATED_FORCEFULLY
Language=English
Service %1 (%2) forcefully terminated child process %3 with exit code %4.
.

MessageId=0x1006
Severity=Informational
Facility=System
SymbolicName=MSG_STOPPED_GRACEFULLY
Language=English
Service %1 (%2) stopped child process %3 gracefully with exit code %4.
.
