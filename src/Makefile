
CXX = g++
LD = g++
WARNINGFLAGS = -Wall -Wextra -Wno-write-strings
CXXFLAGS = -O0 -g -std=gnu++11 -fshort-enums $(WARNINGFLAGS)
LDFLAGS = -lpthread

OBJFILES = mp2.o Daemon.o MembershipList.o net_types.o socket.o utils.o
EXE = mp2

$(EXE): $(OBJFILES)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: clean $(EXE).hpp

clean:
	@rm $(OBJFILES) $(EXE)

