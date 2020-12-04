/***************************************************************************
    copyright            : (C) 2003 by Michael Margraf
                               2018, 2019, 2020 Felix Salfelder / QUCS team
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "sckt_proto.h"
#include "qucs.h"
#include "schematic_doc.h"
#include "misc.h"

#include <QTextStream>
#include <QFileInfo>
#include <QMutex>

#include <limits.h>
#include <io_trace.h>
#include "globals.h"
#include "docfmt.h"
#include "module.h"
#include "components/component.h" // BUG


namespace {

// BUG: must derive from subckt_model (or so)
// Subcircuit is used as "subcircuit instance"..
// but a subcircuit instance is not a Component...
class Subcircuit : public Component /*BUG*/ {
public:
  Subcircuit();
 ~Subcircuit() {};

private:
  Subcircuit(Subcircuit const&x);
  Component* newOne();

public: // obsolete.
  static Element* info(QString&, char* &, bool getNewOne=false);

private:
  Element* clone() const override{ return new Subcircuit(*this); }

public:
  QString getSubcircuitFile(SchematicModel const* scope) const;

protected:
  QString netlist() const;
  QString vhdlCode(int);
  void createSymbol(); // SchematicModel const& scope);
  void defaultSymbol(int No);
  int loadSymbol(const QString&);

private: // Symbol
// unsigned numPorts() const override;
  bool portExists(unsigned) const override;
  std::string const& portName(unsigned) const override;

  // void setParameter(QString const& name, QString const& value);
  void setParameter(unsigned i, std::string const& value) override;
  unsigned paramCount() const override;

  std::string paramName(unsigned i) const override;
  std::string paramValue(unsigned i) const override{
	  if(i == num_component_params + Symbol::paramCount()){
		  // BUG. don't use Props
		  assert(Props.at(0));
		  return Props.at(0)->Value.toStdString();
	  }else{ untested();
		  return Component::paramName(i);
	  }
  }
  std::string paramValue(std::string const& x) const override{
	  trace1("Subcircuit::paramValue", x);
	  incomplete();
	  return Component::paramValue(x);
  }

private: // overrides
	void build() override;

	void proto(SchematicModel const* schem);
	void build_sckt(SubcktBase* proto) const;
}; // Subcircuit
/*--------------------------------------------------------------------------*/
Subcircuit::Subcircuit() : Component() // gaah sckt_base
{
  Type = isComponent;   // both analog and digital
  Description = QObject::tr("subcircuit");

//  Props.append(new Property("File", "", false,
//		QObject::tr("name of qucs schematic file")));

  setTypeName("Sub");

  // Do NOT call createSymbol() here. But create port to let it rotate.
  Ports.append(new ComponentPort(0, 0, false));

  new_subckt(); // triggers sckt expansion
}
/*--------------------------------------------------------------------------*/
std::string Subcircuit::paramName(unsigned i) const
{
  if(i==num_component_params + Symbol::paramCount()){
	  return "File";
  }else if(i==num_component_params + Symbol::paramCount()+1){ untested();
	  return "porttype"; // or so
  }else{
	  return Component::paramName(i);
  }
}
/*--------------------------------------------------------------------------*/
unsigned Subcircuit::paramCount() const
{
  return 1 + Symbol::paramCount() + num_component_params;
}
/*--------------------------------------------------------------------------*/
Subcircuit::Subcircuit(Subcircuit const&x) : Component(x)
{
  Props.append(new Property("File", "", false,
		QObject::tr("name of qucs schematic file")));

  setTypeName("Sub");

  new_subckt(); // triggers sckt expansion
}
/*--------------------------------------------------------------------------*/
Component* Subcircuit::newOne()
{ untested();
	unreachable();
	Subcircuit *p = new Subcircuit();
	p->setLabel("-newOne-"); // TODO
	p->Props.getFirst()->Value = Props.getFirst()->Value;
	trace1("Subcircuit::clone", Props.getFirst()->Value);

	// p->recreate(0); // does not make sense.
	return p;
}
/*--------------------------------------------------------------------------*/
#if 0
Element* Subcircuit::info(QString& Name, char* &BitmapFile, bool getNewOne)
{ untested();
	assert(false);
  Name = QObject::tr("Subcircuit");
  BitmapFile = (char *) "subcircuit";

  if(getNewOne) { untested();
	  incomplete();
    Subcircuit *p = new Subcircuit();
	 p->setLabel("-getnewOne-"); // TODO
    p->recreate(0);   // createSymbol() is NOT called in constructor !!!
    return p;
  }
  return 0;
}
#endif

// ---------------------------------------------------------------------
// Makes the schematic symbol subcircuit with the correct number
// of ports.
// TODO: is this used in case the file does not exist?
void Subcircuit::createSymbol() // SchematicModel const& scope)
{
  int No;
  QString FileName(Props.getFirst()->Value);

  qDebug() << "FileName" << FileName; // seems okay, sth like opa227.sch

  if(FileName.length()<4){ unreachable();
	  // throw?
	  return;
  }else{

	  //  trace2("sckt::createSymbol", this, scope);
	  incomplete();
	  assert(owner());
	  auto os = prechecked_cast<Symbol*>(owner());
	  assert(os);
//	  trace1("hmm", typeid(*os).name());
	  assert(os->subckt());
	  FileName = getSubcircuitFile(os->subckt());
	  trace2("getting symbol", FileName, os->subckt());

	  tx = INT_MIN;
	  ty = INT_MIN;
	  // BUG // not cached. why?
	  if(loadSymbol(FileName) > 0) {
		  if(tx == INT_MIN)  tx = x1+4;
		  if(ty == INT_MIN)  ty = y2+4;
		  // remove unused ports
		  QMutableListIterator<ComponentPort *> ip(Ports);
		  ComponentPort *pp;
		  while (ip.hasNext()) {
			  pp = ip.next();
			  if(!pp->avail) { untested();
				  pp = ip.peekNext();
				  ip.remove();
			  }
		  }
	  }else{
		  No = SchematicDoc::testFile(FileName);
		  if(No < 0)  No = 0;
		  Ports.clear();
		  defaultSymbol(No);  // no symbol was found -> create standard symbol
	  }
  }
}
// ---------------------------------------------------------------------
void Subcircuit::defaultSymbol(int No)
{
	// sort of default symbol if there is no symbol section?
	incomplete();
  int h = 30*((No-1)/2) + 15;
  Lines.append(new Line(-15, -h, 15, -h,QPen(Qt::darkBlue,2)));
  Lines.append(new Line( 15, -h, 15,  h,QPen(Qt::darkBlue,2)));
  Lines.append(new Line(-15,  h, 15,  h,QPen(Qt::darkBlue,2)));
  Lines.append(new Line(-15, -h,-15,  h,QPen(Qt::darkBlue,2)));
  Texts.append(new Text(-10, -6,"sub"));

  int i=0, y = 15-h;
  while(i<No) {
    i++;
    Lines.append(new Line(-30,  y,-15,  y,QPen(Qt::darkBlue,2)));
    Ports.append(new ComponentPort(-30,  y));
    Texts.append(new Text(-25,y-14,QString::number(i)));

    if(i == No) break;
    i++;
    Lines.append(new Line( 15,  y, 30,  y,QPen(Qt::darkBlue,2)));
    Ports.append(new ComponentPort( 30,  y));
    Texts.append(new Text( 19,y-14,QString::number(i)));
    y += 60;
  }

  x1 = -30; y1 = -h-2;
  x2 =  30; y2 =  h+2;
  tx = x1+4;
  ty = y2+4;
}

// ---------------------------------------------------------------------
// Loads the symbol for the subcircuit from the schematic file and
// returns the number of painting elements.
int Subcircuit::loadSymbol(const QString& DocName)
{
  QFile file(DocName);
  if(!file.open(QIODevice::ReadOnly))
    return -1;

  QString Line;
  // *****************************************************************
  // To strongly speed up the file read operation the whole file is
  // read into the memory in one piece.
  QTextStream ReadWhole(&file);
  QString FileString = ReadWhole.readAll();
  file.close();
  QTextStream stream(&FileString, QIODevice::ReadOnly);


  // read header **************************
  do {
    if(stream.atEnd()) return -2;
    Line = stream.readLine();
    Line = Line.trimmed();
  } while(Line.isEmpty());

  if(Line.left(16) != "<Qucs Schematic ")  // wrong file type ?
    return -3;

  Line = Line.mid(16, Line.length()-17);
  VersionTriplet SymbolVersion = VersionTriplet(Line);
  if (SymbolVersion > QucsVersion) // wrong version number ?
    return -4;

  // read content *************************
  while(!stream.atEnd()) {
    Line = stream.readLine();
    if(Line == "<Symbol>") break;
  }

  x1 = y1 = INT_MAX;
  x2 = y2 = INT_MIN;

  int z=0, Result;
  while(!stream.atEnd()) {
    Line = stream.readLine();
    if(Line == "</Symbol>") {
      x1 -= 4;   // enlarge component boundings a little
      x2 += 4;
      y1 -= 4;
      y2 += 4;
      return z;      // return number of ports
    }

    Line = Line.trimmed();
    if(Line.at(0) != '<') return -5;
    if(Line.at(Line.length()-1) != '>') return -6;
    Line = Line.mid(1, Line.length()-2); // cut off start and end character
    Result = analyseLine(Line, 1);
    if(Result < 0) return -7;   // line format error
    z += Result;
  }

  return -8;   // field not closed
}

// -------------------------------------------------------
// BUG: obsolete callback
QString Subcircuit::netlist() const
{ untested();
	incomplete();
	return "bla";
#if 0
  QString s = Model+":"+Name;

  // output all node names
  foreach(Port *p1, Ports)
    s += " "+p1->Connection->name();   // node names

  // type for subcircuit
  QString f = misc::properFileName(Props.first()->Value);
  s += " Type=\""+misc::properName(f)+"\"";

  // output all user defined properties
  for(Property *pp = Props.next(); pp != 0; pp = Props.next())
    s += " "+pp->name()+"=\""+pp->Value+"\"";
  return s + '\n';
#endif
}

// -------------------------------------------------------
QString Subcircuit::vhdlCode(int)
{ untested();
	return "bla";
#if 0
  QString f = misc::properFileName(Props.first()->Value);
  QString s = "  " + name() + ": entity Sub_" + misc::properName(f);

  // output all user defined properties
  Property *pr = Props.next();
  if (pr) { untested();
    s += " generic map (";
    s += pr->Value;
    for(pr = Props.next(); pr != 0; pr = Props.next())
      s += ", " + pr->Value;
    s += ")";
  }

  // output all node names
  s += " port map (";
  QListIterator<Port *> iport(Ports);
  Port *pp = iport.next();
  if(pp){ untested();
    s += pp->Connection->name();
  }else{ untested();
  }
  while (iport.hasNext()) { untested();
    pp = iport.next();
    s += ", "+pp->Connection->name();   // node names
  }

  s += ");\n";
  return s;
#endif
}

// -------------------------------------------------------
#if 0
QString Subcircuit::verilogCode(int)
{ untested();
  QString f = misc::properFileName(Props.first()->Value);
  QString s = "  Sub_" + misc::properName(f);

  // output all user defined properties
  Property *pr = Props.next();
  if (pr) { untested();
    s += " #(";
    s += misc::Verilog_Param(pr->Value);
    for(pr = Props.next(); pr != 0; pr = Props.next())
      s += ", " + misc::Verilog_Param(pr->Value);
    s += ")";
  }

  // output all node names
  s +=  " " + name() + " (";
  QListIterator<Port *> iport(Ports);
  Port *pp = iport.next();
  if(pp)
    s += pp->Connection->name();
  while (iport.hasNext()) { untested();
    pp = iport.next();
    s += ", "+pp->Connection->name();   // node names
  }

  s += ");\n";
  return s;
}
#endif

// -------------------------------------------------------
QString Subcircuit::getSubcircuitFile(SchematicModel const* scp) const
{
  trace3("getSubcircuitFile", this, scp, scope());

	if(scp){
	}else{ untested();
		assert(owner());
		// auto scp=scope();
	}
	assert(scp);
  // construct full filename
  QString FileName = Props.getFirst()->Value;

  if (FileName.isEmpty()) { untested();
	  incomplete();
      return misc::properAbsFileName(FileName);
  }else{
  }

  QFileInfo FileInfo(FileName);

  if (FileInfo.exists()) { untested();
      // the file must be an absolute path to a schematic file
     return FileInfo.absoluteFilePath();
  } else {
	  qDebug() << FileName << "doesnt exist";
    // get the complete base name (everything except the last '.'
    // and whatever follows
    QString baseName = FileInfo.completeBaseName();

    // if only a file name is supplied, first check if it is in the
    // same directory as the schematic file it is a part of
    if (FileInfo.fileName () != FileName) { untested();
        // the file has no path information, just the file name
		 qDebug() << "no path info";
	 }else if (scp) {
		 qDebug() << "trying to inherit path from sch";
		 // check if a file of the same name is in the same directory
		 // as the schematic file, if we have a pointer to it, in
		 // which case we use this one
		 QFileInfo schematicFileInfo = scp->getFileInfo ();
		 QFileInfo localFIleInfo (schematicFileInfo.canonicalPath () + "/" + baseName + ".sch");
		 qDebug() << schematicFileInfo.canonicalPath() << "seems to exist";
		 if (localFIleInfo.exists ()) {
			 // return the subcircuit saved in the same directory
			 // as the schematic file
			 return localFIleInfo.absoluteFilePath();
		 }else{ untested();
		 }
	 }else{ untested();
		 qDebug() << "no schematic";
	 }

    // look up the hash table for the schematic file as
    // it does not seem to be an absolute path, this will also
    // search the home directory which is always hashed
    QMutex mutex;
    mutex.lock();
    QString hashsearchresult = "";
    // check if GUI is running and there is something in the search path lookup
    if ( (QucsMain != 0) && !QucsMain->schNameHash.isEmpty() )
      hashsearchresult = QucsMain->schNameHash.value(baseName);
    mutex.unlock();

    if (hashsearchresult.isEmpty()) { untested();
        // the schematic was not found in the hash table, return
        // what would always have been returned in this case
        trace1("got something", FileName);
        return misc::properAbsFileName(FileName);
    } else { untested();
        // we found an entry in the hash table, check it actually still exists
        FileInfo.setFile(hashsearchresult);

        if (FileInfo.exists()) { untested();
            // it does exist so return the absolute file path
            return FileInfo.absoluteFilePath();
        } else { untested();
            // the schematic file does not actually exist, return
            // what would always have been returned in this case
            return misc::properAbsFileName(FileName);
        }
    }
  }
}

static SubMap FileList;

#if 0 // obsolete. just use SubcktProto??
class pr : public SubcktProto{
public:
  	pr(Element const* e) : SubcktProto(e){ untested();
	}
	~pr() { untested(); }

	void build() override{ untested();
#if 0
		assert(owner());
		assert(scope()==subckt());

		qDebug() << "pr::build";

		Subcircuit const* inst=dynamic_cast<Subcircuit const*>(instance());
		Component const* pc=inst;
		assert(inst);

		trace2("gSsf", inst, inst->scope());
		QString f = inst->getSubcircuitFile(inst->scope());
		qDebug() << "sckt" << f;

		// load subcircuit schematic
		QString s=pc->Props.first()->Value;
//		SchematicModel* d=schematicModel();
//		SchematicModel const* dc=schematicModel();
//		_subckt = d; // bug. new sckt?

		// todo: error handling.
		QString scktfilename(f);
		assert(scktfilename!="");
		QFile file(scktfilename);
		qDebug() << "getting sckt definition from" << scktfilename << "type" << s;
		file.open(QIODevice::ReadOnly);
		istream_t pstream(&file);

		auto cmd = command_dispatcher["leg_sch"];
		auto D = prechecked_cast<DocumentFormat const*>(cmd);
		assert(D);
		D->load(pstream, this);

		d->setDevType(s);
#endif
	}

	SchematicModel* scope() override{return _subckt;};

private:
	std::string paramValue(std::string const&) const{ untested();
		incomplete();
		return "incomplete";
	}
	Port& port(unsigned) override{ untested();
		unreachable();
		return *new Port();
	}
	pos_t portPosition(unsigned) const  override{ untested();
		unreachable();
		return pos_t(0,0);
	}
};
#endif
/*--------------------------------------------------------------------------*/
void Subcircuit::build()
{
	incomplete();
 	createSymbol(); // BUG. multiple symbols, but embedded into Component
	                // swap by proper symbols later.
	proto(nullptr);
}
/*--------------------------------------------------------------------------*/
// go to the top.
QucsDoc* getProject(Object const* o)
{
	auto E = dynamic_cast<Element const*>(o);
	assert(E);
	Object* e = E->mutable_owner();

	while(auto E = dynamic_cast<Element*>(e)){
		trace2("project?", e->label(), E->mutable_owner());
		e = E->mutable_owner();
	}
	if(auto d = dynamic_cast<QucsDoc*>(e)){
		return d;
	}else{
		throw Exception("no project, " + o->label());
	}
}
/*--------------------------------------------------------------------------*/
void Subcircuit::proto(SchematicModel const* scope)
{
   auto t = typeName();
	trace1("Subcircuit::proto", typeName());
//	auto p = scope->findProto(t);
   assert(owner());
	Symbol const* p = nullptr;
	try{
		p = dynamic_cast<Symbol const*>(find_looking_out(t));
	}catch(ExceptionCantFind const&){
		incomplete();
	}

	if(p){
		trace1("cached", typeName());
	}else{
		trace2("not cached", typeName(), scope);
//		Symbol const* ownersym = dynamic_cast<Symbol const*>(owner());
//		auto* s = new SubcktProto(this);
		Symbol* ss = symbol_dispatcher.clone("subckt_proto");
		auto s = prechecked_cast<SubcktBase*>(ss);
		assert(s);

#if 1
		assert(owner());
		Object* mutable_owner_hack = const_cast<Object*>(owner());
		s->setOwner(mutable_owner_hack); // BUG. project?
#else
		QucsDoc* project = getProject(this);
		s->setOwner(project); // BUG. project?
#endif
		build_sckt(s);
		assert(!s->subckt());
		assert(s->scope());

		QString t = Props.first()->Value;
		trace2("Subcircuit::proto", t, owner());
		
		s->setLabel(typeName());
		trace2("sckt::proto", typeName(), s->typeName());

		incomplete();
		QucsDoc* project = getProject(this);
		assert(project);
		project->installElement(s);

		p = s;
	}

	{
		auto q = prechecked_cast<Symbol*>(p->clone());
		assert(q);
		assert(q->common() == p->common());
		attach_common(q->mutable_common());
		delete q;
	}

//	_proto = p;?
//	set_type(p->type())?

}

void Subcircuit::build_sckt(SubcktBase* proto) const
{
	assert(owner());

	QString f = getSubcircuitFile(scope());

	// load subcircuit schematic
	QString s = Props.first()->Value;
	// QString s = paramValue("filename"); // or so.
	//		SchematicModel const* dc=schematicModel();
	// _subckt = d; // bug. new proto?

	// todo: error handling.
	QString scktfilename(f);
	assert(scktfilename!="");
	QFile file(scktfilename);
	trace2("sckt definition from", scktfilename, s);
	file.open(QIODevice::ReadOnly);
	istream_t pstream(&file);

	auto cmd = command_dispatcher["leg_sch"];
	auto D = prechecked_cast<DocumentFormat const*>(cmd);
	assert(D);
	D->load(pstream, proto);

	proto->setTypeName(s.toStdString());
}

bool Subcircuit::portExists(unsigned i) const
{ untested();
	assert(scope());
	trace1("Subcircuit::portExists", i);
	incomplete();
	return false;
}

static std::string invalid_="invalid";
std::string const& Subcircuit::portName(unsigned) const
{ untested();
	incomplete();
	// throw?
	return invalid_;
}

static const std::string typesep(":");

void Subcircuit::setParameter(unsigned i, std::string const& value)
{
	QString v = QString::fromStdString(value);
	if(i==num_component_params + Symbol::paramCount()){
		std::string newTypeName = "Sub" + typesep + v.left(v.length()-4).toStdString();
		trace2("Subcircuit::setParameter", v, newTypeName);
		setTypeName(newTypeName);
		trace2("Subcircuit::setParameter2", v, newTypeName);
	}else{ untested();
		incomplete();
	}
}

namespace{
Subcircuit D;
static Dispatcher<Symbol>::INSTALL p(&symbol_dispatcher, "Sub", &D);
static Module::INSTALL pp("stuff", &D);
}

}
