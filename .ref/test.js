var array = [ 1,2,3 ]; // An array with some objects
function x(){
    for( var i = 0; i < array.length; i++ )
    {
        (function(){
            var k=i
            setTimeout( function() {
                console.log('i:',k)
              },1);
        })();
      
    }
}
x();