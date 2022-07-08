# Makefile
# Build all programs with "make all"

all: clientA.cpp clientB.cpp serverA.cpp serverB.cpp serverC.cpp serverM.cpp
		g++ -o clientA clientA.cpp
		g++ -o clientB clientB.cpp
		g++ -o serverA serverA.cpp
		g++ -o serverB serverB.cpp
		g++ -o serverC serverC.cpp
		g++ -o serverM serverM.cpp

serverA:

		./serverA

serverB:

		./serverB

serverC:

		./serverC

serverM:

		./serverM

clean:

		$(RM) clientA
		$(RM) clientB
		$(RM) serverA
		$(RM) serverB
		$(RM) serverC
		$(RM) serverM
