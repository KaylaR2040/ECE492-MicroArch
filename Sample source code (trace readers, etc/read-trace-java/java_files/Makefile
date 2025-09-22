JC = javac
#JFLAGS = -g
.SUFFIXES: .java .class

# List your top-level .java file here
CLASSES = sim.java 


# default rule

default: classes
	@echo "my work is done here..."


# rule for making all classes

classes: $(CLASSES:.java=.class)


# generic rule for converting any .java file to any .class file

.java.class:
	$(JC) $(JFLAGS) $*.java


# type "make clean" to remove all .class files

clean:
	$(RM) *.class
