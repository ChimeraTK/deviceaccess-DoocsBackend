CacheFile Mechanism   {#cacheFile}
===================

This feature is a workaroud to deal with long RegisterCatalogue fetch times for Doocs locations with huge property
lists. It requires a description file to be provided to the backend.

To enable this feature, specify the cacheFile parameter in the applications %ChimeraTK Device Descriptor (**cdd**):

\verbatim
(doocs:XFEL.RF/TIMER/LLA6M?cacheFile=Filename.xml)
\endverbatim

Filename.xml describes the Doocs locations property list. An example file may look like this:

\verbatim
  <?xml version=\"1.0\" encoding=\"UTF-8\"?>
  <catalogue version=\"1.0\">
    <register>
      <name>/DUMMY</name>
      <length>1</length>
      <access_mode></access_mode>
      <doocs_type_id>1</doocs_type_id>
      <!--doocs id: INT-->"
    </register>"
  </catalogue>";
\endverbatim


## Using DoocsBackend to Generate Xml Descriptor File 
The descriptor xml file may be user generated. It is also possible to make the DoocsBackend generate one. The steps below detail
the process:
- Set cacheFile key in the cdd to the desired xml filename (eg. Filename.xml).
- Start the server application and run it till Filename.xml is generated in the working
  directory.
- Exit server application.

## Behaviour Specifics:
1. The DoocsBackend always starts a background task for querying the Doocs location and creating a RegisterCatalogue
   from this information.
2. When a cacheFile xml is provided in the cdd, it is parsed to generate the RegisterCatalogue. The RegisterCatalogue
   fetched by background task in step.1 remains unused (except to replace the xml file on disk - see step.3).
3. In cases where cacheFile parameter is specified in cdd:
    - cacheFile xml on disk is replaced with the RegisterCatalogue info from step.1, once this background task
      completes.
    - cacheFile xml is not modified if server application exits before task in step.1 is complete.
4. If no cacheFile parameter is provided in the cdd, backend waits for the task in step.1 to complete. The generated
   RegisterCatalogue from this task is then used by the application, but will not be cached to disk as an xml file.


