#pragma once
namespace dc {
enum class Status { Ok, NoData, Disconnected, InvalidFrame, EndOfStream, Stopped, Stale, Paused };
inline const char* StatusName(Status s) {
    switch(s) {
        case Status::Ok:return "En vivo"; case Status::Paused:return "Pausado";
        case Status::NoData:return "Esperando datos"; case Status::Disconnected:return "Sin conexión";
        case Status::InvalidFrame:return "Datos inválidos"; case Status::EndOfStream:return "Fin de repetición";
        case Status::Stopped:return "Detenido"; case Status::Stale:return "Datos vencidos";
    } return "Desconocido";
}
}
