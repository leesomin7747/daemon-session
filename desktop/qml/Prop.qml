import QtQuick
import QtQuick.Shapes
Item {
    id: icon
    property string kind: "coffee"
    property color tint: "#e8d6b9"
    implicitWidth: 24; implicitHeight: 24
    Shape {
        width: 32; height: 32; scale: Math.min(icon.width,icon.height)/32
        transformOrigin: Item.TopLeft
        ShapePath {
            fillColor: icon.tint; strokeWidth: -1
            PathSvg { path: ({coffee:"M5 12H23V22Q23 28 14 28Q5 28 5 22Z",
                energy:"M11 4H21Q24 4 24 7V26Q24 29 21 29H11Q8 29 8 26V7Q8 4 11 4Z",
                lure:"M7 8L11 4L16 8L21 4L25 8V27H7Z",
                plane:"M2 16L30 3L23 28L16 20L9 25L10 18Z",
                note:"M6 8H26Q28 8 28 10V24Q28 26 26 26H6Q4 26 4 24V10Q4 8 6 8Z",
                blanket:"M3 9Q9 5 16 9Q23 5 29 9V27H3Z"})[icon.kind] || "" }
        }
        ShapePath {
            fillColor: "transparent"; strokeColor: icon.kind==="ping"||icon.kind==="rope"?icon.tint:"#506243"
            strokeWidth: 1.7; capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: ({coffee:"M23 14H26Q31 19 24 22M10 8V3M17 8V4",
                energy:"M18 9L13 17H18L14 24M10 7H22",lure:"M21 18A5 5 0 1 1 11 18A5 5 0 1 1 21 18",
                plane:"M10 18L30 3L16 20",note:"M5 9L16 18L27 9",blanket:"M4 23H28",
                ping:"M9 16A7 7 0 1 1 23 16A7 7 0 1 1 9 16M3 16H6M26 16H29",
                rope:"M3 21Q12 4 23 14Q30 23 22 27Q15 29 17 22Q18 17 29 20"})[icon.kind] || "" }
        }
    }
}
