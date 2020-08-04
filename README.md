Felix Kugler (01526144)

Aufgabe 1.3 Ray-Tracing von IFS

# Implementierung

Der Algorithmus von Hart et al. wurde als Compute Shader in OpenGL implementiert.
Fraktale können als Liste von Matrizen in einer JSON Datei beschrieben werden. Die Implementierung geht davon aus, dass die bounding sphere eine Einheitskugel am Koordinatenursprung ist. Die Matrizen müssen daran angepasst sein. 

## Fraktalschnitt

Eine Heap Struktur wird verwendet um über die Knoten des Fraktals, gereiht nach ihrer Entfernung, zu iterieren. Zu Beginn wird der Wurzelknoten in den Heap eingefügt. Dann wird wiederholt der näheste Knoten entfernt, seine Kindelemente gegen den Strahl getestet und die getroffenen Kindelemente in den Heap eingefügt, bis entweder keine weiteren Knoten im Heap enthalten sind, alle weiteren Blattknoten hinter dem nähsten bereits gefunden Blattknoten liegen. 
Die normalen der tiefsten 5 Knotenschnitte werden gemittelt und als finale Normale des Fraktschnitts verwendet. Dies ist sehr aufwendig und nicht für alle Strahlen notwendig. Daher wird für den Schattenfühler eine Variante des Algorithmus verwendet, die auf die Berechnung der Normalen, sowie auch die Sortierung der Knoten, verzichtet.

## Kugelschnitt

Der Kugelschnitt ist lediglich für den Schnitt eines beliebigen Strahls mit einer Einheitskugel implementiert. Beliebige Kugeln können geschnitten werden in dem der Strahl mit der inversen Transformationmatrix der Kugel transformiert wird. Da nur die inversen Transformationen benötigt werden, werden diese bereits auf der CPU berechnet und beim Laden des Fraktals auf die GPU kopiert. Den Schnittpunkt wieder in den Weltraum zu transaksieren würde ein Invertieren der akkumulierten Kindtransformationen erforden, daher wird die Tiefe des Schnittpunkts als Verhältniss zur Länge des Richtungsvektors des Strahls im Kindraum berechnet. Dies ist ausreichend um die Schnitte zu sortieren.
Divisionen und Wurzeln werden so weit als möglich hinausgezögert, da diese Operationen ein Vielfaches länger dauern als Additionen und Multiplikationen. Tatsächlich kann der Test, ob ein Schnitt zwischen einer Kugel und einem Strahl vorliegt, alleine durch Additionen und Multiplikationen erreicht werden. Die Folgenden zwei Gleichungen werden dazu ausgenutzt:

`a / b < c == a < c * b` für positive `b`. So können Divisionen vermieden werden.

`sqrt(a) < b == a < b * b` für positive `b`. So können Wurzeln vermieden werden. 

Die Normalisierung eines Vektors kann vermieden werden in dem diese beiden Gleichungen kombiniert werden.
`dot(normalize(x), y) < b == dot(x, y) * dot(x, y) < b * b * dot(x, x)` für positive `b`.

Um den nähsten Schnittepunkt zwischen dem Fraktal und dem Strahl zu finden ist es nicht notwendig die Tiefe des Schnitts exakt zu berechnen. Es reicht eine monotone Abbildung davon zu berechnen. Daher wird nicht die Tiefe selbst sondern ihr Quadrat berechnet. Durch die Verwendung der binomische Formel ergiebt sich folgende Optimierung:
`sqrt(a) - sqrt(b) < c == a + b - 2 * sqrt(a * b) < c * c` für positive `c`. So wird eine Wurzel weniger benötigt.

Der eigentlich Kugelschnitt wird berechnet indem der Kugelmittelpunkt auf eine Ebene, orthogonal zum Strahl, projeziert und dann seine Distanz zum Strahl berechnet wird. Die Tiefe kann aus dieser Distanz über den Hypotenusensatz berechnet werden. 


## Schattierung

Das Phong Bleuchtungsmodell wird zur Schattierung verwendet. In der Szene befindet sich eine Lichtquelle. Für bildpunkte, welche zur Lichtquelle gerichtet sind, wird ein zweiter Strahl zur Schattenberechnung verwendet. Die Parameter des Modells werden aus JSON Dateien geladen.


# Benutzung

Die ausführbare Datei IFSTracing.exe muss im selben Verzeichnis gestartet werden wie die json und glsl Dateien, damit diese vom Programm gefunden werden können.
Nach dem Start wird ein Fraktal geladen und angezeigt. Die Ansicht kann mit gedrückter linker Maustaste gedreht werden. Die Anzahl der Rekursionsschritte lässt sich mit einem Schieberegler einstellen. Des Weiteren kann der Open Button verwendet werden um die mitgelieferten Beispiele zu laden. 